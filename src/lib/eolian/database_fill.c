#include "eo_parser.h"

static void
_db_fill_param(Eina_List **plist, Eo_Param_Def *param)
{
   Eolian_Function_Parameter *p = database_parameter_add(param->type,
                                                         param->value,
                                                         param->name,
                                                         param->comment);
   p->param_dir = param->way;
   *plist = eina_list_append(*plist, p);
   p->nonull = param->nonull;
   param->type = NULL;

   p->base = param->base;
   param->base.file = NULL;
}

static Eina_Bool
_db_fill_params(Eina_List *oplist, Eina_List **plist)
{
   Eo_Param_Def *param;
   Eina_List *l;

   EINA_LIST_FOREACH(oplist, l, param)
     _db_fill_param(plist, param);

   return EINA_TRUE;
}

static Eina_Bool
_db_fill_accessor(Eolian_Function *foo_id, Eo_Class_Def *kls,
                  Eo_Property_Def *prop, Eo_Accessor_Def *accessor)
{
   Eo_Accessor_Param *acc_param;
   Eina_List *l;

   if (accessor->type == SETTER)
     foo_id->type = (foo_id->type == EOLIAN_PROP_GET) ? EOLIAN_PROPERTY
                                                      : EOLIAN_PROP_SET;
   else
     foo_id->type = (foo_id->type == EOLIAN_PROP_SET) ? EOLIAN_PROPERTY
                                                      : EOLIAN_PROP_GET;

   if (accessor->ret && accessor->ret->type)
     {
        if (accessor->type == SETTER)
          {
             foo_id->set_ret_type = accessor->ret->type;
             foo_id->set_ret_val  = accessor->ret->default_ret_val;
             foo_id->set_return_comment = eina_stringshare_ref(accessor->ret->comment);
             foo_id->set_return_warn_unused = accessor->ret->warn_unused;
          }
        else
          {
             foo_id->get_ret_type = accessor->ret->type;
             foo_id->get_ret_val  = accessor->ret->default_ret_val;
             foo_id->get_return_comment = eina_stringshare_ref(accessor->ret->comment);
             foo_id->get_return_warn_unused = accessor->ret->warn_unused;
          }
        accessor->ret->type = NULL;
        accessor->ret->default_ret_val = NULL;
     }

   if (accessor->type == SETTER)
     {
        foo_id->set_description = eina_stringshare_ref(accessor->comment);
        if (accessor->legacy)
          foo_id->set_legacy = eina_stringshare_ref(accessor->legacy);
        foo_id->set_only_legacy = accessor->only_legacy;
     }
   else
     {
        foo_id->get_description = eina_stringshare_ref(accessor->comment);
        if (accessor->legacy)
          foo_id->get_legacy = eina_stringshare_ref(accessor->legacy);
        foo_id->get_only_legacy = accessor->only_legacy;
     }

   EINA_LIST_FOREACH(accessor->params, l, acc_param)
     {
        Eolian_Function_Parameter *desc = (Eolian_Function_Parameter*)
            eolian_function_parameter_get_by_name(foo_id, acc_param->name);

        if (!desc)
          {
             ERR("Error - %s not known as parameter of property %s\n",
                 acc_param->name, prop->name);
             return EINA_FALSE;
          }
        else if (acc_param->is_const)
          {
             if (accessor->type == GETTER)
               desc->is_const_on_get = EINA_TRUE;
             else
               desc->is_const_on_set = EINA_TRUE;
          }
     }

   if (kls->type == EOLIAN_CLASS_INTERFACE)
     {
        if (accessor->type == SETTER)
          foo_id->set_virtual_pure = EINA_TRUE;
        else
          foo_id->get_virtual_pure = EINA_TRUE;
     }

   if (accessor->type == GETTER)
     foo_id->base = accessor->base;
   else
     foo_id->set_base = accessor->base;

   accessor->base.file = NULL;

   return EINA_TRUE;
}

static Eina_Bool
_db_fill_accessors(Eolian_Function *foo_id, Eo_Class_Def *kls,
                   Eo_Property_Def *prop)
{
   Eo_Accessor_Def *accessor;
   Eina_List *l;

   EINA_LIST_FOREACH(prop->accessors, l, accessor)
     if (!_db_fill_accessor(foo_id, kls, prop, accessor)) return EINA_FALSE;

   return EINA_TRUE;
}


static Eina_Bool
_db_fill_property(Eolian_Class *cl, Eo_Class_Def *kls, Eo_Property_Def *prop)
{
   Eolian_Function *foo_id = database_function_new(prop->name,
                                                   EOLIAN_UNRESOLVED);

   foo_id->scope = prop->scope;
   foo_id->is_class = prop->is_class;

   if (!_db_fill_params   (prop->keys  , &(foo_id->keys  ))) goto failure;
   if (!_db_fill_params   (prop->values, &(foo_id->params))) goto failure;
   if (!_db_fill_accessors(foo_id, kls, prop)) goto failure;

   if (!prop->accessors)
     {
        foo_id->type = EOLIAN_PROPERTY;
        if (kls->type == EOLIAN_CLASS_INTERFACE)
          foo_id->get_virtual_pure = foo_id->set_virtual_pure = EINA_TRUE;
        foo_id->base = prop->base;
        prop->base.file = NULL;
     }

   cl->properties = eina_list_append(cl->properties, foo_id);

   return EINA_TRUE;

failure:
   database_function_del(foo_id);
   return EINA_FALSE;
}

static Eina_Bool
_db_fill_properties(Eolian_Class *cl, Eo_Class_Def *kls)
{
   Eo_Property_Def *prop;
   Eina_List *l;

   EINA_LIST_FOREACH(kls->properties, l, prop)
     if (!_db_fill_property(cl, kls, prop)) return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_db_fill_method(Eolian_Class *cl, Eo_Class_Def *kls, Eo_Method_Def *meth)
{
   Eolian_Function *foo_id = database_function_new(meth->name, EOLIAN_METHOD);

   foo_id->scope = meth->scope;

   cl->methods = eina_list_append(cl->methods, foo_id);

   if (meth->ret)
     {
        foo_id->get_ret_type = meth->ret->type;
        foo_id->get_return_comment = eina_stringshare_ref(meth->ret->comment);
        foo_id->get_return_warn_unused = meth->ret->warn_unused;
        foo_id->get_ret_val = meth->ret->default_ret_val;
        meth->ret->type = NULL;
        meth->ret->default_ret_val = NULL;
     }

   foo_id->get_description = eina_stringshare_ref(meth->comment);
   foo_id->get_legacy = eina_stringshare_ref(meth->legacy);
   foo_id->obj_is_const = meth->obj_const;
   foo_id->is_class = meth->is_class;

   if (meth->only_legacy)
     foo_id->get_only_legacy = EINA_TRUE;

   _db_fill_params(meth->params, &(foo_id->params));

   if (kls->type == EOLIAN_CLASS_INTERFACE)
     foo_id->get_virtual_pure = EINA_TRUE;

   foo_id->base = meth->base;
   meth->base.file = NULL;

   return EINA_TRUE;
}

static Eina_Bool
_db_fill_methods(Eolian_Class *cl, Eo_Class_Def *kls)
{
   Eo_Method_Def *meth;
   Eina_List *l;

   EINA_LIST_FOREACH(kls->methods, l, meth)
     if (!_db_fill_method(cl, kls, meth)) return EINA_FALSE;

   return EINA_TRUE;
}

static int
_func_error(Eolian_Class *cl, Eolian_Implement *impl)
{
   ERR("Error - %s%s not known in class %s", impl->full_name,
       eolian_class_name_get(cl), (impl->is_prop_get ? ".get"
              : (impl->is_prop_set ? ".set" : "")));
   return -1;
}

static Eina_Bool
_get_impl_func(Eolian_Class *cl, Eolian_Implement *impl,
               Eolian_Function_Type ftype, Eolian_Function **foo_id)
{
   size_t cllen = strlen(cl->full_name);
   size_t imlen = strlen(impl->full_name);
   const char *imstr = impl->full_name;
   *foo_id = NULL;
   if (imstr[0] == '.')
     ++imstr;
   else if ((imlen > (cllen + 1)) && (*(imstr + cllen) == '.')
        && !strncmp(imstr, cl->full_name, cllen))
     imstr += cllen + 1;
   else
     return EINA_TRUE;
   if (strchr(imstr, '.'))
     return EINA_FALSE;
   impl->klass = cl;
   *foo_id = (Eolian_Function*)eolian_class_function_get_by_name(cl, imstr,
                                                                 ftype);
   impl->foo_id = *foo_id;
   return !!*foo_id;
}

static void
_write_impl(Eolian_Function *fid, Eolian_Implement *impl)
{
   if (impl->is_prop_set)
     fid->set_impl = impl;
   else if (impl->is_prop_get)
     fid->get_impl = impl;
   else
     fid->get_impl = fid->set_impl = impl;
}

static int
_db_fill_implement(Eolian_Class *cl, Eolian_Implement *impl)
{
   const char *impl_name = impl->full_name;

   Eolian_Function *foo_id;
   Eolian_Function_Type ftype = EOLIAN_UNRESOLVED;

   if (impl->is_prop_get)
     ftype = EOLIAN_PROP_GET;
   else if (impl->is_prop_set)
     ftype = EOLIAN_PROP_SET;

   if (!impl_name) return _func_error(cl, impl);

   if (impl->is_virtual)
     {
        foo_id = (Eolian_Function*)eolian_class_function_get_by_name(cl,
          impl_name, ftype);
        if (!foo_id)
          return _func_error(cl, impl);
        if (impl->is_prop_set)
          foo_id->set_virtual_pure = EINA_TRUE;
        else
          foo_id->get_virtual_pure = EINA_TRUE;

        impl->full_name = eina_stringshare_printf("%s.%s", cl->full_name,
                                                  impl_name);
        eina_stringshare_del(impl_name);
        impl_name = impl->full_name;

        _write_impl(foo_id, impl);
     }
   else if (impl->is_auto)
     {
        if (!_get_impl_func(cl, impl, ftype, &foo_id))
          return _func_error(cl, impl);
        if (!foo_id)
          goto pasttags;
        if (impl->is_prop_set)
          foo_id->set_auto = EINA_TRUE;
        else
          foo_id->get_auto = EINA_TRUE;

        _write_impl(foo_id, impl);
     }
   else if (impl->is_empty)
     {
        if (!_get_impl_func(cl, impl, ftype, &foo_id))
          return _func_error(cl, impl);
        if (!foo_id)
          goto pasttags;
        if (impl->is_prop_set)
          foo_id->set_empty = EINA_TRUE;
        else
          foo_id->get_empty = EINA_TRUE;

        _write_impl(foo_id, impl);
     }
   else if (impl->is_class_ctor)
     {
        cl->class_ctor_enable = EINA_TRUE;
        const char *inm = impl_name;
        if (inm[0] == '.') ++inm;
        if (strchr(inm, '.')) goto pasttags;
        Eolian_Function *foo_id = (Eolian_Function*)
                                   eolian_class_function_get_by_name(cl,
                                                                     impl_name,
                                                                     ftype);
        foo_id->ctor_of_classes =
          eina_list_sorted_insert(foo_id->ctor_of_classes,
                                  EINA_COMPARE_CB(strcmp),
                                  eina_stringshare_ref(cl->name));
        INF("XXX Fuction %s (type=%d) marked as constructor of class %s", impl_name, (int)ftype, cl->name);
        return 1;
     }
   else if (impl->is_class_dtor)
     {
        cl->class_dtor_enable = EINA_TRUE;
        return 1;
     }
   else if (!_get_impl_func(cl, impl, ftype, &foo_id))
     return _func_error(cl, impl);

pasttags:
   if (impl_name[0] == '.')
     {
        impl->full_name = eina_stringshare_printf("%s%s", cl->full_name,
                                                  impl_name);
        eina_stringshare_del(impl_name);
     }

   cl->implements = eina_list_append(cl->implements, impl);
   return 0;
}

static void
_db_build_implement(Eolian_Class *cl, Eolian_Function *foo_id)
{
   if (foo_id->type == EOLIAN_PROP_SET)
     {
        if (foo_id->set_impl) return;
     }
   else if (foo_id->type == EOLIAN_PROP_GET)
     {
        if (foo_id->get_impl) return;
     }
   else if (foo_id->get_impl && foo_id->set_impl) return;

   Eolian_Implement *impl = calloc(1, sizeof(Eolian_Implement));

   if (foo_id->type == EOLIAN_PROP_SET)
     impl->base = foo_id->set_base;
   else
     impl->base = foo_id->base;
   eina_stringshare_ref(impl->base.file);

   impl->klass = cl;
   impl->foo_id = foo_id;
   impl->full_name = eina_stringshare_printf("%s.%s", cl->full_name,
                                             foo_id->name);

   if (foo_id->type == EOLIAN_PROPERTY)
     {
        if (foo_id->get_impl)
          {
             impl->is_prop_set = EINA_TRUE;
             foo_id->set_impl = impl;
          }
        else if (foo_id->set_impl)
          {
             impl->is_prop_get = EINA_TRUE;
             foo_id->get_impl = impl;
          }
        else
          foo_id->get_impl = foo_id->set_impl = impl;
     }
   else if (foo_id->type == EOLIAN_PROP_SET)
     {
        impl->is_prop_set = EINA_TRUE;
        foo_id->set_impl = impl;
     }
   else if (foo_id->type == EOLIAN_PROP_GET)
     {
        impl->is_prop_get = EINA_TRUE;
        foo_id->get_impl = impl;
     }
   else
     foo_id->get_impl = foo_id->set_impl = impl;

   cl->implements = eina_list_append(cl->implements, impl);
}

static Eina_Bool
_db_fill_implements(Eolian_Class *cl, Eo_Class_Def *kls)
{
   Eolian_Implement *impl;
   Eolian_Function *foo_id;
   Eina_List *l;

   EINA_LIST_FOREACH(kls->implements, l, impl)
     {
        int ret = _db_fill_implement(cl, impl);
        if (ret < 0) return EINA_FALSE;
        if (ret > 0) continue;
        eina_list_data_set(l, NULL); /* prevent double free */
     }

   EINA_LIST_FOREACH(cl->properties, l, foo_id)
     _db_build_implement(cl, foo_id);

   EINA_LIST_FOREACH(cl->methods, l, foo_id)
     _db_build_implement(cl, foo_id);

   return EINA_TRUE;
}

static void
_db_fill_constructor(Eolian_Class *cl, Eolian_Constructor *ctor)
{
   const char *ctor_name = ctor->full_name;
   if (ctor_name[0] == '.')
     {
        ctor->full_name = eina_stringshare_printf("%s%s", cl->full_name,
                                                  ctor_name);
        eina_stringshare_del(ctor_name);
     }

   Eolian_Function *foo_id = (Eolian_Function*)
     eolian_class_function_get_by_name(cl,
       ctor->full_name + strlen(cl->full_name) + 1, EOLIAN_UNRESOLVED);
   foo_id->ctor_of_classes = eina_list_sorted_insert(foo_id->ctor_of_classes,
                                                     EINA_COMPARE_CB(strcmp),
                                                     cl->name);
   INF("XXX Fuction %s (ctor) marked as constructor of class %s", ctor->full_name + strlen(cl->full_name) + 1, cl->name);
   cl->constructors = eina_list_append(cl->constructors, ctor);
}

static Eina_Bool
_db_fill_constructors(Eolian_Class *cl, Eo_Class_Def *kls)
{
   Eolian_Constructor *ctor;
   Eina_List *l;

   EINA_LIST_FOREACH(kls->constructors, l, ctor)
     {
        _db_fill_constructor(cl, ctor);
        eina_list_data_set(l, NULL); /* prevent double free */
     }
 
   return EINA_TRUE;
}

static Eina_Bool
_db_fill_events(Eolian_Class *cl, Eo_Class_Def *kls)
{
   Eolian_Event *event;
   Eina_List *l;

   EINA_LIST_FOREACH(kls->events, l, event)
     {
        cl->events = eina_list_append(cl->events, event);
        eina_list_data_set(l, NULL); /* prevent double free */
     }

   return EINA_TRUE;
}

static Eina_Bool
_db_fill_class(Eo_Class_Def *kls)
{
   Eolian_Class *cl = database_class_add(kls->name, kls->type);
   const char *s;
   Eina_List *l;

   eina_hash_set(_classesf, kls->base.file, cl);

   if (kls->comment)
     cl->description = eina_stringshare_ref(kls->comment);

   EINA_LIST_FOREACH(kls->inherits, l, s)
     cl->inherits = eina_list_append(cl->inherits, eina_stringshare_add(s));

   if (kls->legacy_prefix)
     cl->legacy_prefix = eina_stringshare_ref(kls->legacy_prefix);
   if (kls->eo_prefix)
     cl->eo_prefix = eina_stringshare_ref(kls->eo_prefix);
   if (kls->data_type)
     cl->data_type = eina_stringshare_ref(kls->data_type);

   if (!_db_fill_properties  (cl, kls)) return EINA_FALSE;
   if (!_db_fill_methods     (cl, kls)) return EINA_FALSE;
   if (!_db_fill_implements  (cl, kls)) return EINA_FALSE;
   if (!_db_fill_constructors(cl, kls)) return EINA_FALSE;
   if (!_db_fill_events      (cl, kls)) return EINA_FALSE;

   cl->base = kls->base;
   kls->base.file = NULL;

   return EINA_TRUE;
}

Eina_Bool
eo_parser_database_fill(const char *filename, Eina_Bool eot)
{
   Eina_List *k;
   Eo_Node *nd;
   Eina_Bool has_class = EINA_FALSE;

   Eo_Lexer *ls = eo_lexer_new(filename);
   if (!ls)
     {
        ERR("unable to create lexer");
        return EINA_FALSE;
     }

   /* read first token */
   eo_lexer_get(ls);

   if (!eo_parser_walk(ls, eot))
     {
        eo_lexer_free(ls);
        return EINA_FALSE;
     }

   if (eot) goto done;

   EINA_LIST_FOREACH(ls->nodes, k, nd) if (nd->type == NODE_CLASS)
     {
        has_class = EINA_TRUE;
        break;
     }

   if (!has_class)
     {
        ERR("No classes for file %s", filename);
        eo_lexer_free(ls);
        return EINA_FALSE;
     }

   EINA_LIST_FOREACH(ls->nodes, k, nd)
     {
        switch (nd->type)
          {
           case NODE_CLASS:
             if (!_db_fill_class(nd->def_class))
               goto error;
             break;
           default:
             break;
          }
     }

done:
   eo_lexer_free(ls);
   return EINA_TRUE;

error:
   eo_lexer_free(ls);
   return EINA_FALSE;
}
