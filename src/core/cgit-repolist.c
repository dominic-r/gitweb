/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "scan-tree.h"
#include "cache.h"
#include "ui-stats.h"
#include "cgit-main.h"

static int cmp_repos(const void *a, const void *b)
{
	const struct cgit_repo *ra = a, *rb = b;
	return strcmp(ra->url, rb->url);
}

static char *build_snapshot_setting(int bitmap)
{
	const struct cgit_snapshot_format *f;
	struct strbuf result = STRBUF_INIT;

	for (f = cgit_snapshot_formats; f->suffix; f++) {
		if (cgit_snapshot_format_bit(f) & bitmap) {
			if (result.len)
				strbuf_addch(&result, ' ');
			strbuf_addstr(&result, f->suffix);
		}
	}
	return strbuf_detach(&result, NULL);
}

static char *get_first_line(char *txt)
{
	char *t = xstrdup(txt);
	char *p = strchr(t, '\n');
	if (p)
		*p = '\0';
	return t;
}

static void print_repo(FILE *f, struct cgit_repo *repo)
{
	struct string_list_item *item;
	fprintf(f, "repo.url=%s\n", repo->url);
	fprintf(f, "repo.name=%s\n", repo->name);
	fprintf(f, "repo.path=%s\n", repo->path);
	if (repo->owner)
		fprintf(f, "repo.owner=%s\n", repo->owner);
	if (repo->desc) {
		char *tmp = get_first_line(repo->desc);
		fprintf(f, "repo.desc=%s\n", tmp);
		free(tmp);
	}
	for_each_string_list_item(item, &repo->readme) {
		if (item->util)
			fprintf(f, "repo.readme=%s:%s\n", (char *)item->util,
				item->string);
		else
			fprintf(f, "repo.readme=%s\n", item->string);
	}
	if (repo->defbranch)
		fprintf(f, "repo.defbranch=%s\n", repo->defbranch);
	if (repo->extra_head_content)
		fprintf(f, "repo.extra-head-content=%s\n", repo->extra_head_content);
	if (repo->module_link)
		fprintf(f, "repo.module-link=%s\n", repo->module_link);
	if (repo->section)
		fprintf(f, "repo.section=%s\n", repo->section);
	if (repo->homepage)
		fprintf(f, "repo.homepage=%s\n", repo->homepage);
	if (repo->clone_url)
		fprintf(f, "repo.clone-url=%s\n", repo->clone_url);
	fprintf(f, "repo.enable-blame=%d\n", repo->enable_blame);
	fprintf(f, "repo.enable-commit-graph=%d\n", repo->enable_commit_graph);
	fprintf(f, "repo.enable-log-filecount=%d\n", repo->enable_log_filecount);
	fprintf(f, "repo.enable-log-linecount=%d\n", repo->enable_log_linecount);
	if (repo->about_filter && repo->about_filter != ctx.cfg.about_filter)
		cgit_fprintf_filter(repo->about_filter, f, "repo.about-filter=");
	if (repo->commit_filter && repo->commit_filter != ctx.cfg.commit_filter)
		cgit_fprintf_filter(repo->commit_filter, f, "repo.commit-filter=");
	if (repo->source_filter && repo->source_filter != ctx.cfg.source_filter)
		cgit_fprintf_filter(repo->source_filter, f, "repo.source-filter=");
	if (repo->email_filter && repo->email_filter != ctx.cfg.email_filter)
		cgit_fprintf_filter(repo->email_filter, f, "repo.email-filter=");
	if (repo->owner_filter && repo->owner_filter != ctx.cfg.owner_filter)
		cgit_fprintf_filter(repo->owner_filter, f, "repo.owner-filter=");
	if (repo->snapshots != ctx.cfg.snapshots) {
		char *tmp = build_snapshot_setting(repo->snapshots);
		fprintf(f, "repo.snapshots=%s\n", tmp ? tmp : "");
		free(tmp);
	}
	if (repo->snapshot_prefix)
		fprintf(f, "repo.snapshot-prefix=%s\n", repo->snapshot_prefix);
	if (repo->max_stats != ctx.cfg.max_stats)
		fprintf(f, "repo.max-stats=%s\n",
			cgit_find_stats_periodname(repo->max_stats));
	if (repo->logo)
		fprintf(f, "repo.logo=%s\n", repo->logo);
	if (repo->logo_link)
		fprintf(f, "repo.logo-link=%s\n", repo->logo_link);
	fprintf(f, "repo.enable-remote-branches=%d\n",
		repo->enable_remote_branches);
	fprintf(f, "repo.enable-subject-links=%d\n", repo->enable_subject_links);
	fprintf(f, "repo.enable-html-serving=%d\n", repo->enable_html_serving);
	if (repo->branch_sort == 1)
		fprintf(f, "repo.branch-sort=age\n");
	if (repo->commit_sort) {
		if (repo->commit_sort == 1)
			fprintf(f, "repo.commit-sort=date\n");
		else if (repo->commit_sort == 2)
			fprintf(f, "repo.commit-sort=topo\n");
	}
	fprintf(f, "repo.hide=%d\n", repo->hide);
	fprintf(f, "repo.ignore=%d\n", repo->ignore);
	fprintf(f, "\n");
}

static void print_repolist(FILE *f, struct cgit_repolist *list, int start)
{
	int i;

	for (i = start; i < list->count; i++)
		print_repo(f, &list->repos[i]);
}

/* Scan 'path' for git repositories, save the resulting repolist in 'cached_rc'
 * and return 0 on success.
 */
static int generate_cached_repolist(const char *path, const char *cached_rc)
{
	struct strbuf locked_rc = STRBUF_INIT;
	int result = 0;
	int idx;
	FILE *f;

	strbuf_addf(&locked_rc, "%s.lock", cached_rc);
	f = fopen(locked_rc.buf, "wx");
	if (!f) {
		/* Inform about the error unless the lockfile already existed,
		 * since that only means we've got concurrent requests.
		 */
		result = errno;
		if (result != EEXIST)
			fprintf(stderr, "[cgit] Error opening %s: %s (%d)\n",
				locked_rc.buf, strerror(result), result);
		goto out;
	}
	idx = cgit_repolist.count;
	if (ctx.cfg.project_list)
		scan_projects(path, ctx.cfg.project_list, cgit_repo_config);
	else
		scan_tree(path, cgit_repo_config);
	print_repolist(f, &cgit_repolist, idx);
	if (rename(locked_rc.buf, cached_rc))
		fprintf(stderr, "[cgit] Error renaming %s to %s: %s (%d)\n",
			locked_rc.buf, cached_rc, strerror(errno), errno);
	fclose(f);
out:
	strbuf_release(&locked_rc);
	return result;
}

void cgit_process_cached_repolist(const char *path)
{
	struct stat st;
	struct strbuf cached_rc = STRBUF_INIT;
	time_t age;
	unsigned long hash;

	hash = hash_str(path);
	if (ctx.cfg.project_list)
		hash += hash_str(ctx.cfg.project_list);
	strbuf_addf(&cached_rc, "%s/rc-%8lx", ctx.cfg.cache_root, hash);

	if (stat(cached_rc.buf, &st)) {
		/* Nothing is cached, we need to scan without forking. And
		 * if we fail to generate a cached repolist, we need to
		 * invoke scan_tree manually.
		 */
		if (generate_cached_repolist(path, cached_rc.buf)) {
			if (ctx.cfg.project_list)
				scan_projects(path, ctx.cfg.project_list,
					      cgit_repo_config);
			else
				scan_tree(path, cgit_repo_config);
		}
		goto out;
	}

	cgit_parse_config_file(cached_rc.buf);

	/* If the cached configfile hasn't expired, lets exit now */
	age = time(NULL) - st.st_mtime;
	if (age <= (ctx.cfg.cache_scanrc_ttl * 60))
		goto out;

	/* The cached repolist has been parsed, but it was old. So lets
	 * rescan the specified path and generate a new cached repolist
	 * in a child-process to avoid latency for the current request.
	 */
	if (fork())
		goto out;

	exit(generate_cached_repolist(path, cached_rc.buf));
out:
	strbuf_release(&cached_rc);
}

void cgit_parse_args(int argc, const char **argv)
{
	int i;
	const char *arg;
	int scan = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--version")) {
			printf("CGit %s | https://github.com/dominic-r/gitweb\n\nCompiled in features:\n", cgit_version);
#ifdef NO_LUA
			printf("[-] ");
#else
			printf("[+] ");
#endif
			printf("Lua scripting\n");
#ifndef HAVE_LINUX_SENDFILE
			printf("[-] ");
#else
			printf("[+] ");
#endif
			printf("Linux sendfile() usage\n");

			exit(0);
		}
		if (skip_prefix(argv[i], "--cache=", &arg)) {
			ctx.cfg.cache_root = xstrdup(arg);
		} else if (!strcmp(argv[i], "--nohttp")) {
			ctx.env.no_http = "1";
		} else if (skip_prefix(argv[i], "--query=", &arg)) {
			ctx.qry.raw = xstrdup(arg);
		} else if (skip_prefix(argv[i], "--repo=", &arg)) {
			ctx.qry.repo = xstrdup(arg);
		} else if (skip_prefix(argv[i], "--page=", &arg)) {
			ctx.qry.page = xstrdup(arg);
		} else if (skip_prefix(argv[i], "--head=", &arg)) {
			ctx.qry.head = xstrdup(arg);
			ctx.qry.has_symref = 1;
		} else if (skip_prefix(argv[i], "--oid=", &arg)) {
			ctx.qry.oid = xstrdup(arg);
			ctx.qry.has_oid = 1;
		} else if (skip_prefix(argv[i], "--ofs=", &arg)) {
			ctx.qry.ofs = atoi(arg);
		} else if (skip_prefix(argv[i], "--scan-tree=", &arg) ||
			   skip_prefix(argv[i], "--scan-path=", &arg)) {
			/*
			 * HACK: The global snapshot bit mask defines the set
			 * of allowed snapshot formats, but the config file
			 * hasn't been parsed yet so the mask is currently 0.
			 * By setting all bits high before scanning we make
			 * sure that any in-repo cgitrc snapshot setting is
			 * respected by scan_tree().
			 *
			 * NOTE: We assume that there aren't more than 8
			 * different snapshot formats supported by cgit...
			 */
			ctx.cfg.snapshots = 0xFF;
			scan++;
			scan_tree(arg, cgit_repo_config);
		}
	}
	if (scan) {
		qsort(cgit_repolist.repos, cgit_repolist.count,
			sizeof(struct cgit_repo), cmp_repos);
		print_repolist(stdout, &cgit_repolist, 0);
		exit(0);
	}
}
