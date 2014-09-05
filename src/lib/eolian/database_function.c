#include <Eina.h>
#include "eolian_database.h"

void
database_function_del(Eolian_Function *fid)
{
   Eolian_Function_Parameter *param;
   Eina_Stringshare *cl_name;
   if (!fid) return;

   if (fid->base.file) eina_stringshare_del(fid->base.file);
   eina_stringshare_del(fid->name);
   EINA_LIST_FREE(fid->keys, param) database_parameter_del(param);
   EINA_LIST_FREE(fid->params, param) database_parameter_del(param);
   EINA_LIST_FREE(fid->ctor_of_classes, cl_name) eina_stringshare_del(cl_name);
   database_type_del(fid->get_ret_type);
   database_type_del(fid->set_ret_type);
   database_expr_del(fid->get_ret_val);
   database_expr_del(fid->set_ret_val);
   if (fid->get_legacy) eina_stringshare_del(fid->get_legacy);
   if (fid->set_legacy) eina_stringshare_del(fid->set_legacy);
   if (fid->get_description) eina_stringshare_del(fid->get_description);
   if (fid->set_description) eina_stringshare_del(fid->set_description);
   if (fid->get_return_comment) eina_stringshare_del(fid->get_return_comment);
   if (fid->set_return_comment) eina_stringshare_del(fid->set_return_comment);
   free(fid);
}

Eolian_Function *
database_function_new(const char *function_name, Eolian_Function_Type foo_type)
{
   Eolian_Function *fid = calloc(1, sizeof(*fid));
   fid->name = eina_stringshare_add(function_name);
   fid->type = foo_type;
   return fid;
}
