
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Eo.h>
#include <stdlib.h>

#include "simple.eo.h"

#define MY_CLASS SIMPLE_CLASS

struct _Simple_Data {};
typedef struct _Simple_Data Simple_Data;

static void _simple_eo_base_constructor(Eo *obj, Simple_Data *data EINA_UNUSED)
{
  eo_do_super(obj, MY_CLASS, eo_constructor());
}

#include "simple.eo.c"

