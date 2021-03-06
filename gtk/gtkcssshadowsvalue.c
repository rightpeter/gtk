/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssshadowsvalueprivate.h"

#include "gtkcssshadowvalueprivate.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint         len;
  GtkCssValue  *values[1];
};

static GtkCssValue *    gtk_css_shadows_value_new       (GtkCssValue **values,
                                                         guint         len);

static void
gtk_css_value_shadows_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->len; i++)
    {
      _gtk_css_value_unref (value->values[i]);
    }

  g_slice_free1 (sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (value->len - 1), value);
}

static gboolean
gtk_css_value_shadows_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  guint i;

  /* XXX: Should we fill up here? */
  if (value1->len != value2->len)
    return FALSE;

  for (i = 0; i < value1->len; i++)
    {
      if (!_gtk_css_value_equal (value1->values[i],
                                 value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_shadows_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  double       progress)
{
  GtkCssValue *result;
  guint i;

  /* catches the important case of 2 none values */
  if (start == end)
    return _gtk_css_value_ref (start);

  if (start->len > end->len)
    result = gtk_css_shadows_value_new (start->values, start->len);
  else
    result = gtk_css_shadows_value_new (end->values, end->len);

  for (i = 0; i < MIN (start->len, end->len); i++)
    {
      result->values[i] = _gtk_css_value_transition (start->values[i], end->values[i], progress);
    }
  if (start->len > end->len)
    {
      for (; i < result->len; i++)
        {
          GtkCssValue *fill = _gtk_css_shadow_value_new_for_transition (start->values[i]);
          result->values[i] = _gtk_css_value_transition (start->values[i], fill, progress);
          _gtk_css_value_unref (fill);
        }
    }
  else
    {
      for (; i < result->len; i++)
        {
          GtkCssValue *fill = _gtk_css_shadow_value_new_for_transition (end->values[i]);
          result->values[i] = _gtk_css_value_transition (fill, end->values[i], progress);
          _gtk_css_value_unref (fill);
        }
    }

  return result;
}

static void
gtk_css_value_shadows_print (const GtkCssValue *value,
                             GString           *string)
{
  guint i;

  if (value->len == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->len; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _gtk_css_value_print (value->values[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOWS = {
  gtk_css_value_shadows_free,
  gtk_css_value_shadows_equal,
  gtk_css_value_shadows_transition,
  gtk_css_value_shadows_print
};

static GtkCssValue none_singleton = { &GTK_CSS_VALUE_SHADOWS, 1, 0, { NULL } };

GtkCssValue *
_gtk_css_shadows_value_new_none (void)
{
  return _gtk_css_value_ref (&none_singleton);
}

static GtkCssValue *
gtk_css_shadows_value_new (GtkCssValue **values,
                           guint         len)
{
  GtkCssValue *result;
           
  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (len > 0, NULL);
         
  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_SHADOWS, sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (len - 1));
  result->len = len;
  memcpy (&result->values[0], values, sizeof (GtkCssValue *) * len);
            
  return result;
}

GtkCssValue *
_gtk_css_shadows_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value, *result;
  GPtrArray *values;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_css_shadows_value_new_none ();

  values = g_ptr_array_new ();

  do {
    value = _gtk_css_shadow_value_parse (parser);

    if (value == NULL)
      {
        g_ptr_array_set_free_func (values, (GDestroyNotify) _gtk_css_value_unref);
        g_ptr_array_free (values, TRUE);
        return NULL;
      }

    g_ptr_array_add (values, value);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  result = gtk_css_shadows_value_new ((GtkCssValue **) values->pdata, values->len);
  g_ptr_array_free (values, TRUE);
  return result;
}

GtkCssValue *
_gtk_css_shadows_value_compute (GtkCssValue     *value,
                                GtkStyleContext *context)
{
  GtkCssValue *result;
  guint i;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_SHADOWS, NULL);

  if (value->len == 0)
    return _gtk_css_value_ref (value);

  result = gtk_css_shadows_value_new (value->values, value->len);
  for (i = 0; i < value->len; i++)
    {
      result->values[i] = _gtk_css_shadow_value_compute (value->values[i], context);
    }

  return result;
}

void
_gtk_css_shadows_value_paint_layout (const GtkCssValue *shadows,
                                     cairo_t           *cr,
                                     PangoLayout       *layout)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _gtk_css_shadow_value_paint_layout (shadows->values[i], cr, layout);
    }
}

void
_gtk_css_shadows_value_paint_icon (const GtkCssValue *shadows,
                                   cairo_t           *cr)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _gtk_css_shadow_value_paint_icon (shadows->values[i], cr);
    }
}

void
_gtk_css_shadows_value_paint_spinner (const GtkCssValue *shadows,
                                      cairo_t           *cr,
                                      gdouble            radius,
                                      gdouble            progress)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _gtk_css_shadow_value_paint_spinner (shadows->values[i], cr, radius, progress);
    }
}

void
_gtk_css_shadows_value_paint_box (const GtkCssValue   *shadows,
                                  cairo_t             *cr,
                                  const GtkRoundedBox *padding_box)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      _gtk_css_shadow_value_paint_box (shadows->values[i], cr, padding_box);
    }
}
