/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 30-Dec-2011
 */

/**
   @addtogroup xcode

   @{
 */

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>                         /* strcpy, strcat, strrchr */

#include "xcode/ff2c/lex.h"
#include "xcode/ff2c/parser.h"
#include "xcode/ff2c/sem.h"
#include "xcode/ff2c/gen.h"

FILE *thefile;

#define out(...) fprintf(thefile, __VA_ARGS__)

static void indent(int depth)
{
	static const char ruler[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

	out("%*.*s", depth, depth, ruler);
}

#define TOK(tok) (int)(tok)->ft_len, (int)(tok)->ft_len, (tok)->ft_val
#define T(term) TOK(&(term)->fn_tok)

static void type_h(const struct ff2c_type *t, int depth);

static void field_h(const struct ff2c_field *f, int depth)
{
	if (f->f_decl != NULL) {
		indent(depth);
		out("%s", f->f_decl);
	} else {
		type_h(f->f_type, depth);
		out(" %s", f->f_name);
	}
	if (f->f_parent->t_array)
		out("[%s]", f->f_tag);
	out(";\n");
}

static void type_h(const struct ff2c_type *t, int depth)
{
	const struct ff2c_field *f = t->t_field.l_head;

	indent(depth);
	out("%s {\n", t->t_c_name);
	if (t->t_union) {
		field_h(f, depth + 1);
		f = f->f_next;
		indent(depth + 1);
		out("union {\n");
		depth++;
	}
	for (; f != NULL; f = f->f_next)
		field_h(f, depth + 1);
	if (t->t_union) {
		indent(depth);
		out("} u;\n");
		depth--;
	}
	indent(depth);
	out("}");
}

int ff2c_h_gen(const struct ff2c_ff *ff, const struct ff2c_gen_opt *opt)
{
	const struct ff2c_require *r;
	const struct ff2c_type    *t;

	thefile = opt->go_out;

	out("/* This file is automatically generated from %s.ff */\n\n",
	    opt->go_basename);
	out("#pragma once\n\n");
	out("#ifndef %s\n"
	    "#define %s\n\n", opt->go_guardname, opt->go_guardname);
	out("#ifndef __KERNEL__\n");
	out("#include <sys/types.h>\n");
	out("#endif /* __KERNEL__ */\n\n");
	out("#include \"xcode/xcode.h\"\n\n");

	for (r = ff->ff_require.l_head; r != NULL; r = r->r_next)
		out("#include %s\n", r->r_path);
	out("\n");
	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		if (t->t_public) {
			type_h(t, 0);
			out(";\n\n");
		}
	}
	out("\n");
	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		if (t->t_public)
			out("extern struct m0_xcode_type *%s;\n", t->t_xc_name);
	}
	out("\n"
	    "M0_INTERNAL void m0_xc_%s_init(void);\n"
	    "M0_INTERNAL void m0_xc_%s_fini(void);\n\n", opt->go_basename,
	    opt->go_basename);
	out("/* %s */\n"
	    "#endif\n\n", opt->go_guardname);
	return 0;
}

static void field_def(const struct ff2c_type *t,
		      const struct ff2c_field *f, int i)
{
	out("\t_%s._child[%i] = (struct m0_xcode_field) {\n"
	    "\t\t.xf_name   = \"%s\",\n"
	    "\t\t.xf_type   = %s,\n"
	    "\t\t.xf_tag    = %s,\n"
	    "\t\t.xf_opaque = %s,\n"
	    "\t\t.xf_offset = offsetof(%s, %s)\n"
	    "\t};\n",
	    t->t_name, i, f->f_name, f->f_xc_type,
	    f->f_tag ?: "0",
	    f->f_escape ?: "NULL",
	    t->t_c_name, f->f_c_name);
}

static void type_fields(const struct ff2c_type *t)
{
	const struct ff2c_field *f;
	int                      i;

	for (i = 0, f = t->t_field.l_head; f != NULL; f = f->f_next, i++)
		field_def(t, f, i);
}

static void type_decl(const struct ff2c_type *t)
{
	out("%sstruct m0_xcode_type *%s",
	    t->t_public ? "" : "static ", t->t_xc_name);
}

static void type_def(const struct ff2c_type *t)
{
	static const char *caggr[] = {
		[FTT_VOID]     = "M0_XA_ATOM",
		[FTT_U8]       = "M0_XA_ATOM",
		[FTT_U32]      = "M0_XA_ATOM",
		[FTT_U64]      = "M0_XA_ATOM",
		[FTT_OPAQUE]   = "M0_XA_OPAQUE",
		[FTT_RECORD]   = "M0_XA_RECORD",
		[FTT_UNION]    = "M0_XA_UNION",
		[FTT_SEQUENCE] = "M0_XA_SEQUENCE",
		[FTT_ARRAY]    = "M0_XA_ARRAY"
	};
	out("static struct _%s_s {\n"
	    "\tstruct m0_xcode_type _type;\n"
	    "\tstruct m0_xcode_field _child[%i];\n"
	    "} _%s = {\n"
	    "\t._type = {\n"
	    "\t\t.xct_aggr   = %s,\n"
	    "\t\t.xct_name   = \"%s\",\n"
	    "\t\t.xct_sizeof = sizeof (%s),\n"
	    "\t\t.xct_nr     = %i\n"
	    "\t}\n"
	    "};\n\n",
	    t->t_name, t->t_nr, t->t_name,
	    caggr[t->t_term->fn_head->fn_tok.ft_type],
	    t->t_name, t->t_c_name, t->t_nr);
	type_decl(t);
	out(" = &_%s._type;\n"
	    "M0_BASSERT(offsetof(struct _%s_s, _child[0]) ==\n"
	    "\toffsetof(struct m0_xcode_type, xct_child[0]));\n\n",
	    t->t_name, t->t_name);
}

int ff2c_c_gen(const struct ff2c_ff *ff, const struct ff2c_gen_opt *opt)
{
	const struct ff2c_type   *t;
	const struct ff2c_escape *e;

	thefile = opt->go_out;

	out("/* This file is automatically generated from %s.ff */\n\n",
	    opt->go_basename);

	out("#include \"lib/misc.h\"                       /* offsetof */\n");
	out("#include \"lib/assert.h\"\n");
	out("#include \"xcode/xcode.h\"\n\n");
	out("#include \"%s_ff.h\"\n\n", opt->go_basename);

	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		if (t->t_public) {
			type_decl(t);
			out(";\n");
		}
	}
	out("\n");
	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		if (!t->t_public) {
			type_decl(t);
			out(";\n");
		}
	}
	out("\n");
	for (e = ff->ff_escape.l_head; e != NULL; e = e->e_next) {
		out("int %s(const struct m0_xcode_obj *par,\n"
		    "\t\tconst struct m0_xcode_type **out);\n", e->e_escape);
	}
	out("\n");
	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next)
		type_def(t);

	out("\n\n"
	    "M0_INTERNAL void m0_xc_%s_init(void)\n"
	    "{\n", opt->go_basename);

	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		type_fields(t);
		out("\n");
	}
	out("}\n"
	    "M0_INTERNAL void m0_xc_%s_fini(void)\n{}\n", opt->go_basename);

	return 0;
}

/** @} end of xcode group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
