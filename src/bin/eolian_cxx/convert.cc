
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstddef>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Eina.hh>
#include <Eolian.h>

#include "eo_types.hh"
#include "eo_validate.hh"

#include "safe_strings.hh"
#include "convert_comments.hh"
#include "eolian_wrappers.hh"

namespace eolian_cxx {

extern efl::eina::log_domain domain;

static efl::eolian::parameters_container_type
_convert_eolian_parameters(Eina_Iterator *parameters,
                          Eolian_Function_Type func_type)
{
   if (parameters == NULL) return {};
   assert(func_type != EOLIAN_PROPERTY);

   void *curr;
   efl::eolian::parameters_container_type list;
   EINA_ITERATOR_FOREACH(parameters, curr)
     {
        const Eolian_Function_Parameter *id =
          (static_cast<const Eolian_Function_Parameter*>(curr));
        list.push_back
          ({
             parameter_type(*id, func_type),
             parameter_name(*id)
          });
     }
   eina_iterator_free(parameters);
   return list;
}

static efl::eolian::parameters_container_type
_convert_eolian_parameters(Eolian_Function const& func, getter_t func_type)
{
   return _convert_eolian_parameters
     (::eolian_function_parameters_get(&func), func_type.value);
}

static efl::eolian::parameters_container_type
_convert_eolian_parameters(Eina_Iterator *parameters, getter_t func_type)
{
   return _convert_eolian_parameters(parameters, func_type.value);
}

static efl::eolian::parameters_container_type
_convert_eolian_parameters(Eina_Iterator *parameters, setter_t func_type)
{
   return _convert_eolian_parameters(parameters, func_type.value);
}

static efl::eolian::parameters_container_type
_convert_eolian_parameters(Eolian_Function const& func)
{
   assert(function_op_type(func) != EOLIAN_PROPERTY);
   return _convert_eolian_parameters
     (::eolian_function_parameters_get(&func), function_op_type(func));
}

static efl::eolian::functions_container_type
_convert_eolian_property_to_functions(Eolian_Class const& klass)
{
   efl::eolian::functions_container_type container;
   std::string cxx_classname = safe_lower(class_name(klass));
   Eina_Iterator *properties =
     ::eolian_class_functions_get(&klass, EOLIAN_PROPERTY); // XXX
   void *curr;
   std::string prefix(class_prefix(klass));
   EINA_ITERATOR_FOREACH(properties, curr)
     {
        Eolian_Function *prop_ = static_cast<Eolian_Function*>(curr);
        if (property_is_getter(*prop_))
          {
             efl::eolian::parameters_container_type params
               = _convert_eolian_parameters(*prop_, eolian_cxx::getter);

             efl::eolian::eo_function get_;
             get_.type = efl::eolian::eo_function::regular_;
             get_.name = function_name(*prop_) + "_get";
             get_.impl = function_impl(*prop_, prefix) + "_get";

             efl::eolian::eolian_type_instance ret =
               function_return_type(*prop_, eolian_cxx::getter);

             // if the getter has a single parameter and a void return
             // it is transformed into a getter with no parameters
             // that actually returns what would be the first argument.
             if (params.size() == 1 && efl::eolian::type_is_void(ret) &&
                 !function_return_is_explicit_void(*prop_, eolian_cxx::getter))
               {
                  get_.ret = params[0].type;
                  get_.params.clear();
               }
             else // otherwise just create the described getter
               {
                  get_.ret = ret;
                  get_.params = params;
                  std::transform
                    (params.begin(), params.end(), get_.params.begin(),
                     [](efl::eolian::eo_parameter const& param)
                     {
                        efl::eolian::eolian_type getter_param_type =
                          type_to_native(param.type);
                        getter_param_type.native += "*";
                        return efl::eolian::eo_parameter
                          { { getter_param_type }, param.name };
                     });
               }
             efl::eolian::parameters_container_type keys =
               _convert_eolian_parameters(::eolian_property_keys_get(prop_),
                                         eolian_cxx::getter);
             if (!keys.empty())
               {
                  keys.reserve(keys.size() + get_.params.size());
                  keys.insert(keys.end(), get_.params.begin(),
                              get_.params.end());
                  get_.params = keys;
               }
             get_.comment = convert_comments_function(klass, *prop_, eolian_cxx::getter);
             container.push_back(get_);
          }
        if (property_is_setter(*prop_))
          {
             efl::eolian::eo_function set_;
             set_.type = efl::eolian::eo_function::regular_;
             set_.name = function_name(*prop_) + "_set";
             set_.impl = function_impl(*prop_, prefix) + "_set";
             efl::eolian::parameters_container_type keys =
               _convert_eolian_parameters(::eolian_property_keys_get(prop_),
                                         eolian_cxx::setter);
             efl::eolian::parameters_container_type params =
               _convert_eolian_parameters(::eolian_function_parameters_get(prop_),
                                         eolian_cxx::setter);
             set_.params = params;
             if (!keys.empty())
               {
                  keys.reserve(keys.size() + set_.params.size());
                  keys.insert(keys.end(), set_.params.begin(),
                              set_.params.end());
                  set_.params = keys;
               }
             set_.ret = function_return_type(*prop_, eolian_cxx::setter);
             set_.comment = convert_comments_function(klass, *prop_, eolian_cxx::setter);
             container.push_back(set_);
          }
     }
   eina_iterator_free(properties);
   return container;
}

static std::string
_convert_class_name(std::string const& name)
{
  // "eo_base" is the Eolian name for EO_BASE_CLASS.
  return (name == "eo_base" || name == "eo::base" || name == "")
    ? "efl::eo::base"
    : name;
}

void
convert_eolian_inheritances(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   Eina_Iterator *inheritances =
     ::eolian_class_inherits_get(&klass);
   void *curr;

   if (!eina_iterator_next(inheritances, &curr) || !curr)
     {
        cls.parent = "efl::eo::base";
        eina_iterator_free(inheritances);
        return;
     }
   else
     {
        // First element is the parent
        const char *ptr = static_cast<const char*>(curr);
        cls.parent = _convert_class_name(class_format_cxx(safe_lower(ptr)));

        EINA_ITERATOR_FOREACH(inheritances, curr)
         {
           std::string extension = safe_lower(static_cast<const char*>(curr));
           cls.extensions.push_back(_convert_class_name(class_format_cxx(extension)));
         }
     }
   eina_iterator_free(inheritances);
}

void
convert_eolian_constructors(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   efl::eina::iterator<const Eolian_Constructor> it = class_ctors(klass);
   efl::eina::iterator<const Eolian_Constructor> end;
   while (it != end)
     {
        Eolian_Function const& func = constructor_function(*it);
        efl::eolian::eo_constructor ctor_ =
        {
           function_impl(func, class_prefix(klass)),
           _convert_eolian_parameters(func),
           convert_comments_function(klass, func)
        };
        cls.constructors.push_back(ctor_);
        ++it;
     }
}

void
convert_eolian_functions(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   efl::eina::iterator_iterator<const Eolian_Function> funcs
      (::eolian_class_functions_get(&klass, EOLIAN_METHOD));

   for (auto it = funcs.begin(), end = funcs.end(); it != end; ++it)
     {
        if (function_is_constructing(klass, *it) || !function_is_generated(*it))
           continue;
        efl::eolian::eo_function func_;
        func_.type = function_type(*it);
        func_.name = function_name(*it);
        func_.impl = function_impl(*it, class_prefix(klass));
        func_.ret = function_return_type(*it);
        func_.params = _convert_eolian_parameters(*it);
        func_.comment = convert_comments_function(klass, *it, eolian_cxx::method);
        cls.functions.push_back(func_);
     }
}

void
convert_eolian_properties(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   efl::eolian::functions_container_type properties
     = _convert_eolian_property_to_functions(klass);
   cls.functions.insert
     (cls.functions.end(), properties.begin(), properties.end());
}

void
convert_eolian_events(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   efl::eolian::events_container_type events = event_list(klass);
   cls.events.reserve(cls.events.size() + events.size());
   cls.events.insert(cls.events.end(), events.begin(), events.end());
}

efl::eolian::eo_class
convert_eolian_class_new(Eolian_Class const& klass)
{
   efl::eolian::eo_class cls;
   cls.type = class_type(klass);
   cls.name = safe_lower(class_name(klass));
   cls.name_space = class_namespace_full(klass);
   cls.eo_name = class_eo_name(klass);
   cls.comment = convert_comments_class(klass);
   return cls;
}

void
convert_eolian_implements(efl::eolian::eo_class& cls, Eolian_Class const& klass)
{
   std::string prefix(class_prefix(klass));
   Eina_Iterator *itr = ::eolian_class_implements_get(&klass);
   void *impl_desc_;

   EINA_ITERATOR_FOREACH(itr, impl_desc_)
     {
        const Eolian_Implement *impl_desc = static_cast<Eolian_Implement*>(impl_desc_);
        const Eolian_Function *impl_func = implement_function(*impl_desc);
        assert(!!impl_func);
        assert(!!implement_class(*impl_desc));
        if (!function_is_constructing(*implement_class(*impl_desc), *impl_func) ||
            !function_is_generated(*impl_func))
           continue;
        std::string parent = safe_lower
        (class_full_name(*implement_class(*impl_desc)));
        if (parent == "eo.base") parent = "eo"; 
        efl::eolian::eo_constructor ctor_ =
        {
           parent + "_" + function_name(*impl_func),
           _convert_eolian_parameters(*impl_func),
           convert_comments_function(klass, *impl_func)
        };
        cls.constructors.push_back(ctor_);
     }
   eina_iterator_free(itr);
}

efl::eolian::eo_class
convert_eolian_class(const Eolian_Class& klass)
{
   efl::eolian::eo_class cls(eolian_cxx::convert_eolian_class_new(klass));
   eolian_cxx::convert_eolian_inheritances(cls, klass);
   eolian_cxx::convert_eolian_implements(cls, klass);
   eolian_cxx::convert_eolian_constructors(cls, klass);
   eolian_cxx::convert_eolian_functions(cls, klass);
   eolian_cxx::convert_eolian_properties(cls, klass);
   eolian_cxx::convert_eolian_events(cls, klass);
   efl::eolian::eo_class_validate(cls);
   return cls;
}

} // namespace eolian_cxx {
