/* ui-compare.c: compare two refs (GitHub-style compare view)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-compare.h"
#include "ui-diff.h"
#include "ui-log.h"
#include "ui-shared.h"
#include "html.h"
#include "strvec.h"

static void print_compare_form(const char *base_ref, const char *head_ref)
{
	char *url;

	html("<div class='compare-form'>");
	html("<form method='get' action='");
	if (ctx.cfg.virtual_root) {
		url = cgit_pageurl(ctx.repo->url, "compare", NULL);
		html_attr(url);
		free(url);
	}
	html("'>\n");
	if (!ctx.cfg.virtual_root) {
		struct strbuf form_url = STRBUF_INIT;
		strbuf_addf(&form_url, "%s/%s", ctx.qry.repo, "compare");
		html_hidden("url", form_url.buf);
		strbuf_release(&form_url);
	}
	if (ctx.qry.head && ctx.repo->defbranch &&
	    strcmp(ctx.qry.head, ctx.repo->defbranch))
		html_hidden("h", ctx.qry.head);
	html("<table><tr>");
	html("<td><input type='text' name='id' value='");
	if (base_ref)
		html_attr(base_ref);
	html("' placeholder='base' size='20'/></td>");
	html("<td> ... </td>");
	html("<td><input type='text' name='id2' value='");
	if (head_ref)
		html_attr(head_ref);
	html("' placeholder='compare' size='20'/></td>");
	html("<td><input type='submit' value='Compare'/></td>");
	html("</tr></table>");
	html("</form>");
	html("</div>");
}

static void print_commit_row(struct commit *commit)
{
	struct commitinfo *info;

	info = cgit_parse_commit(commit);
	html("<tr>");
	html("<td>");
	cgit_print_age(info->committer_date, info->committer_tz, TM_WEEK * 2);
	html("</td>");
	html("<td>");
	cgit_commit_link(info->subject, NULL, NULL, ctx.qry.head,
			 oid_to_hex(&commit->object.oid), NULL);
	show_commit_decorations(commit);
	html("</td>");
	html("<td>");
	cgit_open_filter(ctx.repo->email_filter, info->author_email, "compare");
	html_txt(info->author);
	cgit_close_filter(ctx.repo->email_filter);
	html("</td>");
	html("</tr>\n");
	cgit_free_commitinfo(info);
}

static int print_commit_list(const char *base_ref, const char *head_ref)
{
	struct rev_info rev;
	struct commit *commit;
	struct strvec rev_argv = STRVEC_INIT;
	int count = 0;

	strvec_push(&rev_argv, "compare");
	strvec_pushf(&rev_argv, "%s..%s", base_ref, head_ref);
	strvec_push(&rev_argv, "--");

	repo_init_revisions(the_repository, &rev, NULL);
	rev.abbrev = DEFAULT_ABBREV;
	rev.commit_format = CMIT_FMT_DEFAULT;
	rev.verbose_header = 1;
	rev.show_root_diff = 0;
	rev.ignore_missing = 1;
	rev.simplify_history = 1;
	setup_revisions(rev_argv.nr, rev_argv.v, &rev, NULL);
	load_ref_decorations(NULL, DECORATE_FULL_REFS);
	rev.show_decorations = 1;

	if (prepare_revision_walk(&rev)) {
		cgit_print_error("Error preparing revision walk");
		strvec_clear(&rev_argv);
		return -1;
	}

	html("<div class='compare-commits'>");
	html("<table class='list nowrap'>");
	html("<tr class='nohover'>");
	html("<th class='left'>Age</th>");
	html("<th class='left'>Commit message</th>");
	html("<th class='left'>Author</th>");
	html("</tr>\n");

	while ((commit = get_revision(&rev)) != NULL) {
		print_commit_row(commit);
		count++;
		release_commit_memory(the_repository->parsed_objects, commit);
		commit->parents = NULL;
	}

	html("</table>");
	html("</div>");

	strvec_clear(&rev_argv);
	return count;
}

void cgit_print_compare(void)
{
	const char *base_ref = NULL;
	const char *head_ref = NULL;
	struct object_id base_oid, head_oid;
	struct commit *base_commit, *head_commit;
	char *vpath;
	char *sep;

	/* Parse vpath: split on "..." then ".." */
	vpath = ctx.qry.vpath ? xstrdup(ctx.qry.vpath) : NULL;
	if (vpath) {
		sep = strstr(vpath, "...");
		if (sep) {
			*sep = '\0';
			base_ref = vpath;
			head_ref = sep + 3;
		} else {
			sep = strstr(vpath, "..");
			if (sep) {
				*sep = '\0';
				base_ref = vpath;
				head_ref = sep + 2;
			}
		}
		/* If either side is empty after split, treat as unset */
		if (base_ref && !*base_ref)
			base_ref = NULL;
		if (head_ref && !*head_ref)
			head_ref = NULL;
	}

	/* Fallback to query params (ignore empty strings) */
	if (!base_ref && ctx.qry.oid && *ctx.qry.oid)
		base_ref = ctx.qry.oid;
	if (!head_ref && ctx.qry.oid2 && *ctx.qry.oid2)
		head_ref = ctx.qry.oid2;

	cgit_print_layout_start();

	/* Always show the form */
	print_compare_form(base_ref, head_ref);

	if (!base_ref || !head_ref) {
		if (base_ref || head_ref)
			cgit_print_error("Please provide both base and compare refs.");
		cgit_print_layout_end();
		free(vpath);
		return;
	}

	/* Resolve refs */
	if (repo_get_oid(the_repository, base_ref, &base_oid)) {
		cgit_print_error("Bad object name: %s", base_ref);
		cgit_print_layout_end();
		free(vpath);
		return;
	}
	base_commit = lookup_commit_reference(the_repository, &base_oid);
	if (!base_commit || repo_parse_commit(the_repository, base_commit)) {
		cgit_print_error("Bad commit: %s", base_ref);
		cgit_print_layout_end();
		free(vpath);
		return;
	}

	if (repo_get_oid(the_repository, head_ref, &head_oid)) {
		cgit_print_error("Bad object name: %s", head_ref);
		cgit_print_layout_end();
		free(vpath);
		return;
	}
	head_commit = lookup_commit_reference(the_repository, &head_oid);
	if (!head_commit || repo_parse_commit(the_repository, head_commit)) {
		cgit_print_error("Bad commit: %s", head_ref);
		cgit_print_layout_end();
		free(vpath);
		return;
	}

	/* Compare info header */
	html("<div class='compare-info'>");
	htmlf("Comparing ");
	html("<strong>");
	html_txt(base_ref);
	html("</strong>");
	htmlf(" ... ");
	html("<strong>");
	html_txt(head_ref);
	html("</strong>");
	html("</div>");

	/* Commit list */
	print_commit_list(base_ref, head_ref);

	/* Diff: reuse cgit_print_diff with show_ctrls=0 */
	cgit_print_diff(oid_to_hex(&head_oid), oid_to_hex(&base_oid),
			NULL, 0, 0);

	cgit_print_layout_end();
	free(vpath);
}
