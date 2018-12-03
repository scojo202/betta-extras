#include <string.h>
#include <math.h>
#include <stdio.h>
#include <y-scalar-property.h>

/**
 * SECTION: y-scalar-property
 * @short_description: A scalar data object that reflects the value of a GObject property.
 *
 * Data classes #YPropertyScalar
 *
 */

/**
 * YPropertyScalar:
 *
 * YScalar that changes when a property changes.
 **/

struct _YPropertyScalar {
	YScalar base;
	GObject *obj;
	gchar *name;
};

G_DEFINE_TYPE (YPropertyScalar, y_property_scalar, Y_TYPE_SCALAR);

static void
y_property_scalar_finalize (GObject *obj)
{
	YPropertyScalar *s = (YPropertyScalar *)obj;
	g_signal_handlers_disconnect_by_data(s->obj,s);
	g_object_unref(s->obj);
	g_free(s->name);

	GObjectClass *obj_class = G_OBJECT_CLASS(y_property_scalar_parent_class);
	(*obj_class->finalize) (obj);
}

static double
y_property_scalar_get_value (YScalar *dat)
{
	YPropertyScalar const *ps = (YPropertyScalar const *)dat;
	g_return_val_if_fail(ps->obj,NAN);
	GObjectClass *klass = G_OBJECT_GET_CLASS(ps->obj);
	GParamSpec *spec = g_object_class_find_property(klass,ps->name);
	double val = NAN;
	if(spec->value_type == G_TYPE_DOUBLE) {
		g_object_get(ps->obj,ps->name,&val,NULL);
	}
	if(spec->value_type == G_TYPE_FLOAT) {
		float fval;
		g_object_get(ps->obj,ps->name,&fval,NULL);
		val = (double)fval;
	}
	if(spec->value_type == G_TYPE_INT) {
		int ival;
		g_object_get(ps->obj,ps->name,&ival,NULL);
		val = (double)ival;
	}
	if(spec->value_type == G_TYPE_UINT) {
		unsigned int uval;
		g_object_get(ps->obj,ps->name,&uval,NULL);
		val = (double)uval;
	}
	return val;
}

static void
y_property_scalar_class_init (YPropertyScalarClass *klass)
{
	GObjectClass *gobject_klass = (GObjectClass *) klass;
	gobject_klass->finalize = y_property_scalar_finalize;
	//YDataClass *ydata_klass = (YDataClass *) klass;
	YScalarClass *scalar_klass = (YScalarClass *) klass;
	//ydata_klass->dup  = y_property_scalar_dup;
	scalar_klass->get_value  = y_property_scalar_get_value;
}

static void
y_property_scalar_init(YPropertyScalar *val)
{}

static
void on_notify (GObject    *gobject,
                                                        GParamSpec *pspec,
                                                        gpointer    user_data)
{
	YData *d = Y_DATA(user_data);
	y_data_emit_changed(d);
}

/**
 * y_property_scalar_new:
 * @obj: a GObject
 * @name: the property name
 *
 * Creates a new #YPropertyScalar object. The property must be numeric.
 *
 * Returns: (transfer full): The new object.
 **/
YPropertyScalar	*y_property_scalar_new (GObject *obj, const gchar *name)
{
	/* check that there is a property with this name */
	GObjectClass *class = G_OBJECT_GET_CLASS(obj);
	GParamSpec *spec = g_object_class_find_property(class,name);
	g_return_val_if_fail(spec!=NULL,NULL);
	/* ensure that the property is a numeric type */
	GType type = G_PARAM_SPEC_VALUE_TYPE(spec);
	g_return_val_if_fail(type==G_TYPE_DOUBLE || type==G_TYPE_FLOAT || type==G_TYPE_INT || type==G_TYPE_UINT,NULL);

	YPropertyScalar *p = g_object_new(Y_TYPE_PROPERTY_SCALAR,NULL);
	p->obj = g_object_ref(obj);
	p->name = g_strdup(name);
	/* connect to notify signal and emit changed */
	gchar *notify = g_malloc(10+strlen(name));
	sprintf(notify,"notify::%s",p->name);
	g_signal_connect(p->obj, notify, G_CALLBACK(on_notify), p);
	g_free(notify);
	return p;
}
