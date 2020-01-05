#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

enum {
	P_AIN = 0,
	P_AOUT,
	P_LAST,
};

typedef struct {
	float* ports[P_LAST];
} Dbg;

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	Dbg* self = (Dbg*)calloc (1, sizeof (Dbg));
	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	Dbg* self = (Dbg*)instance;
	if (port < P_LAST) {
		self->ports[port] = (float*)data;
	}
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	Dbg*         self = (Dbg*)instance;
	const float* in   = self->ports[P_AIN];
	float*       out  = self->ports[P_AOUT];

	if (out != in) {
		memcpy (out, in, n_samples * sizeof (float));
	}
}

static void
cleanup (LV2_Handle instance)
{
	free (instance);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	"http://gareus.org/oss/lv2/dbg",
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data
};

#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
#define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#define LV2_SYMBOL_EXPORT __attribute__ ((visibility ("default")))
#endif
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
