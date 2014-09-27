
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Eo.hh>
#include <check.h>

extern "C"
{
#include "simple.eo.h"
}

START_TEST(eo_cxx_ref_basic)
{
   efl::eo::eo_init eo_init;

   Eo *r1 = ::eo_ref(eo_add(SIMPLE_CLASS, NULL));
   efl::eo::base o1(eo_add(SIMPLE_CLASS, r1));

   efl::eina::optional<efl::eo::base> p1 = o1.parent_get();
   ck_assert(p1.is_engaged());
   ck_assert(*p1 == efl::eo::base(r1));

   ck_assert(::eo_ref_get(r1) == 2);
   ck_assert(o1.ref_get() == 2);
   
   ::eo_ref(r1);

   ck_assert(::eo_ref_get(r1) == 3);
   ck_assert(o1.ref_get() == 2);

   o1.parent_set(nullptr);
   ck_assert(o1.ref_get() == 1);
   
   ::eo_del(r1);

   r1 = ::eo_ref(eo_add(SIMPLE_CLASS, NULL));
   o1.parent_set(efl::eo::base(r1));
   ck_assert(o1.ref_get() == 2);
   ::eo_del(r1);
   ck_assert(o1.ref_get() == 1);
   ck_assert(::eo_ref_get(r1) == 0);

   p1 = o1.parent_get();
   ck_assert(!p1.is_engaged());
}
END_TEST

void
eo_cxx_test_ref(TCase *tc)
{
   tcase_add_test(tc, eo_cxx_ref_basic);
}
