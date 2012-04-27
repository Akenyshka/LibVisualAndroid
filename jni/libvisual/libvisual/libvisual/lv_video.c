/* Libvisual - The audio visualisation framework.
 *
 * Copyright (C) 2004, 2005, 2006 Dennis Smit <ds@nerds-incorporated.org>
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 *	    Duilio J. Protti <dprotti@users.sourceforge.net>
 *	    Chong Kai Xiong <kaixiong@codeleft.sg>
 *	    Jean-Christophe Hoelt <jeko@ios-software.com>
 *	    Jaak Randmets <jaak.ra@gmail.com>
 *
 * $Id: lv_video.c,v 1.86.2.1 2006/03/04 12:32:47 descender Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <config.h>
#include "lv_video.h"
#include "lv_color.h"
#include "lv_common.h"
#include "lv_cpu.h"
#include "private/lv_video_convert.h"
#include "private/lv_video_fill.h"
#include "private/lv_video_scale.h"
#include "private/lv_video_blit.h"
#include "gettext.h"

#pragma pack(1)

typedef struct {
#if VISUAL_LITTLE_ENDIAN == 1
	uint16_t b:5, g:6, r:5;
#else
	uint16_t r:5, g:6, b:5;
#endif
} rgb16_t;

#pragma pack()

/* The VisVideo dtor function */
static int video_dtor (VisObject *object);

/* Precomputation functions */
static void precompute_row_table (VisVideo *video);

/* Rotate functions */
static void rotate_90 (VisVideo *dest, VisVideo *src);
static void rotate_180 (VisVideo *dest, VisVideo *src);
static void rotate_270 (VisVideo *dest, VisVideo *src);

/* Mirror functions */
static void mirror_x (VisVideo *dest, VisVideo *src);
static void mirror_y (VisVideo *dest, VisVideo *src);

static int video_dtor (VisObject *object)
{
	VisVideo *video = VISUAL_VIDEO (object);

	visual_color_free (video->colorkey);

	visual_rectangle_free (video->rect);

	if (video->pixel_rows != NULL)
		visual_mem_free (video->pixel_rows);

	if (video->parent != NULL)
		visual_object_unref (VISUAL_OBJECT (video->parent));

	if (video->buffer != NULL)
		visual_buffer_free (video->buffer);

	video->pixel_rows = NULL;
	video->parent = NULL;
	video->buffer = NULL;

	return VISUAL_OK;
}


VisVideo *visual_video_new ()
{
	VisVideo *video;

	video = visual_mem_new0 (VisVideo, 1);

	visual_video_init (video);

	/* Do the VisObject initialization */
	visual_object_set_allocated (VISUAL_OBJECT (video), TRUE);
	visual_object_ref (VISUAL_OBJECT (video));

	return video;
}

void visual_video_init (VisVideo *video)
{
	visual_return_if_fail (video != NULL);

	/* Do the VisObject initialization */
	visual_object_clear (VISUAL_OBJECT (video));
	visual_object_set_dtor (VISUAL_OBJECT (video), video_dtor);
	visual_object_set_allocated (VISUAL_OBJECT (video), FALSE);

	/* Reset the VisVideo data */
	video->buffer = visual_buffer_new ();

	video->pixel_rows = NULL;

	visual_video_set_attributes (video, 0, 0, 0, VISUAL_VIDEO_DEPTH_NONE);
	visual_video_set_buffer (video, NULL);
	visual_video_set_palette (video, NULL);

	video->parent = NULL;
	video->rect = visual_rectangle_new_empty ();

	/* Compose control */
	video->compose_type = VISUAL_VIDEO_COMPOSE_TYPE_SRC;

	/* Colors */
	video->colorkey = visual_color_new ();
}

VisVideo *visual_video_new_with_buffer (int width, int height, VisVideoDepth depth)
{
	VisVideo *video;

	video = visual_video_new ();

	visual_video_set_depth (video, depth);
	visual_video_set_dimension (video, width, height);

	if (!visual_video_allocate_buffer (video)) {
		visual_object_unref (VISUAL_OBJECT (video));
		return NULL;
	}

	return video;
}

void visual_video_free_buffer (VisVideo *video)
{
	visual_return_if_fail (video != NULL);
	visual_return_if_fail (visual_video_get_pixels (video) != NULL);

	if (video->pixel_rows != NULL)
		visual_mem_free (video->pixel_rows);

	if (!visual_buffer_is_allocated (video->buffer))
        return;

    visual_buffer_destroy_content (video->buffer);

	video->pixel_rows = NULL;
	visual_buffer_set_data_pair (video->buffer, NULL, 0);
}

int visual_video_allocate_buffer (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, FALSE);
	visual_return_val_if_fail (video->buffer != NULL, FALSE);

	if (visual_video_get_pixels (video) != NULL) {
		if (visual_buffer_is_allocated (video->buffer)) {
			visual_video_free_buffer (video);
		} else {
			visual_log (VISUAL_LOG_ERROR, _("Trying to allocate an screen buffer on "
					"a VisVideo structure which points to an external screen buffer"));
			return FALSE;
		}
	}

	if (visual_video_get_size (video) == 0) {
		visual_buffer_set_data (video->buffer, NULL);
		return FALSE;
	}

	visual_buffer_set_size (video->buffer, visual_video_get_size (video));
	visual_buffer_allocate_data (video->buffer);

	video->pixel_rows = visual_mem_new0 (void *, video->height);
	precompute_row_table (video);

    return TRUE;
}

int visual_video_has_allocated_buffer (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, FALSE);

	return visual_buffer_is_allocated (video->buffer);
}

static void precompute_row_table (VisVideo *video)
{
	uint8_t **table, *row;
	int y;

	visual_return_if_fail (video->pixel_rows != NULL);

	table = (uint8_t **) video->pixel_rows;
	row = (uint8_t *) visual_video_get_pixels (video);

	for (y = 0; y < video->height; y++, row += video->pitch)
		*table++ = row;
}

void visual_video_copy_attrs (VisVideo *dest, VisVideo *src)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	visual_video_set_depth (dest, src->depth);
	visual_video_set_dimension (dest, src->width, src->height);
	visual_video_set_pitch (dest, src->pitch);
}

int visual_video_compare_attrs (VisVideo *src1, VisVideo *src2)
{
	visual_return_val_if_fail (src1 != NULL, FALSE);
	visual_return_val_if_fail (src2 != NULL, FALSE);

	if (!visual_video_compare_attrs_ignore_pitch (src1, src2))
		return FALSE;

	if (src1->pitch != src2->pitch)
		return FALSE;

	/* We made it to the end, the VisVideos are likewise in depth, pitch, dimensions */
	return TRUE;
}

int visual_video_compare_attrs_ignore_pitch (VisVideo *src1, VisVideo *src2)
{
	visual_return_val_if_fail (src1 != NULL, FALSE);
	visual_return_val_if_fail (src2 != NULL, FALSE);

	if (src1->depth != src2->depth)
		return FALSE;

	if (src1->width != src2->width)
		return FALSE;

	if (src1->height != src2->height)
		return FALSE;

	/* We made it to the end, the VisVideos are likewise in depth, pitch, dimensions */
	return TRUE;
}

void visual_video_set_palette (VisVideo *video, VisPalette *pal)
{
	visual_return_if_fail (video != NULL);

	if (video->pal) {
		visual_palette_free (video->pal);
	}

	video->pal = pal ? visual_palette_clone (pal) : NULL;
}

void visual_video_set_buffer (VisVideo *video, void *buffer)
{
	visual_return_if_fail (video != NULL);

	if (visual_buffer_is_allocated (video->buffer)) {
		visual_log (VISUAL_LOG_ERROR, _("Trying to set a screen buffer on "
                                        "a VisVideo structure which points to an allocated screen buffer"));

		return;
	}

	visual_buffer_set_data (video->buffer, buffer);

	if (video->pixel_rows) {
		visual_mem_free (video->pixel_rows);

		video->pixel_rows = NULL;
	}

	if (visual_buffer_get_data (video->buffer)) {
		video->pixel_rows = visual_mem_new0 (void *, video->height);

		precompute_row_table (video);
	}
}

void visual_video_set_dimension (VisVideo *video, int width, int height)
{
	visual_return_if_fail (video != NULL);

	video->width  = width;
	video->height = height;
	video->pitch  = video->width * video->bpp;

	visual_buffer_set_size (video->buffer, video->pitch * video->height);
}

void visual_video_set_pitch (VisVideo *video, int pitch)
{
	visual_return_if_fail (video != NULL);
	visual_return_if_fail (pitch > 0);

	if (video->bpp <= 0)
		return;

	video->pitch = pitch;
	visual_buffer_set_size (video->buffer, video->pitch * video->height);
}

void visual_video_set_depth (VisVideo *video, VisVideoDepth depth)
{
	visual_return_if_fail (video != NULL);

	video->depth = depth;
	video->bpp = visual_video_bpp_from_depth (video->depth);
}

void visual_video_set_attributes (VisVideo *video, int width, int height, int pitch, VisVideoDepth depth)
{
	visual_return_if_fail (video != NULL);

	visual_video_set_depth (video, depth);
	visual_video_set_dimension (video, width, height);
	visual_video_set_pitch (video, pitch);
}

visual_size_t visual_video_get_size (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, 0);

	return video->pitch * video->height;
}

void *visual_video_get_pixels (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, NULL);

	return visual_buffer_get_data (video->buffer);
}

VisBuffer *visual_video_get_buffer (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, NULL);

	return video->buffer;
}

VisRectangle *visual_video_get_extents (VisVideo *video)
{
	visual_return_val_if_fail (video != NULL, NULL);

	return visual_rectangle_new (0, 0, video->width, video->height);
}

void visual_video_region_sub (VisVideo *dest, VisVideo *src, VisRectangle *area)
{
	VisRectangle *vrect = NULL;
	int area_x, area_y, area_width, area_height;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);
	visual_return_if_fail (area != NULL);

	visual_return_if_fail (!visual_rectangle_is_empty (area));

	vrect = visual_video_get_extents (src);

	if (!visual_rectangle_contains_rect (vrect, area)) {
		goto out;
	}

	visual_rectangle_copy (dest->rect, area);
	visual_object_ref (VISUAL_OBJECT (src));

	dest->parent = src;

	area_x      = visual_rectangle_get_x (area);
	area_y      = visual_rectangle_get_y (area);
	area_width  = visual_rectangle_get_width  (area);
	area_height = visual_rectangle_get_height (area);

	visual_video_set_attributes (dest, area_width, area_height,
			(area_width * src->bpp) + (src->pitch - (area_width * src->bpp)), src->depth);
	visual_video_set_buffer (dest, (uint8_t *) (visual_video_get_pixels (src)) + ((area_y * src->pitch) + (area_x * src->bpp)));

	/* Copy compose */
	dest->compose_type = src->compose_type;
	dest->compose_func = src->compose_func;
	visual_color_copy (dest->colorkey, src->colorkey);
	dest->alpha = src->alpha;

	visual_video_set_palette (dest, src->pal);

out:
	if (vrect) visual_rectangle_free (vrect);
}

void visual_video_region_sub_by_values (VisVideo *dest, VisVideo *src, int x, int y, int width, int height)
{
	VisRectangle *rect = NULL;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	rect = visual_rectangle_new (x, y, width, height);

	visual_video_region_sub (dest, src, rect);

	visual_rectangle_free (rect);
}

void visual_video_region_sub_all (VisVideo *dest, VisVideo *src)
{
	VisRectangle *rect = NULL;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	rect = visual_video_get_extents (dest);

	visual_video_region_sub (dest, src, rect);

	visual_rectangle_free (rect);
}

void visual_video_region_sub_with_boundary (VisVideo *dest, VisRectangle *drect, VisVideo *src, VisRectangle *srect)
{
	VisRectangle *rsrect = NULL;
	VisRectangle *sbound = NULL;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);
	visual_return_if_fail (drect != NULL);
	visual_return_if_fail (srect != NULL);

	rsrect = visual_rectangle_clone (srect);
	sbound = visual_video_get_extents (src);

	/* Merge the destination and source rect, so that only the allowed parts are sub regioned */
	visual_rectangle_clip (rsrect, sbound, srect);
	visual_rectangle_clip (rsrect, drect, rsrect);

	visual_video_region_sub (dest, src, rsrect);

	visual_rectangle_free (sbound);
	visual_rectangle_free (rsrect);
}

void visual_video_set_compose_type (VisVideo *video, VisVideoComposeType type)
{
	visual_return_if_fail (video != NULL);

	video->compose_type = type;

}

void visual_video_set_compose_colorkey (VisVideo *video, VisColor *color)
{
	visual_return_if_fail (video != NULL);

	if (color != NULL)
		visual_color_copy (video->colorkey, color);
	else
		visual_color_set (video->colorkey, 0, 0, 0);
}

void visual_video_set_compose_surface (VisVideo *video, uint8_t alpha)
{
	visual_return_if_fail (video != NULL);

	video->alpha = alpha;
}

void visual_video_set_compose_function (VisVideo *video, VisVideoComposeFunc compose_func)
{
	visual_return_if_fail (video != NULL);

	video->compose_func = compose_func;
}

VisVideoComposeFunc visual_video_get_compose_function (VisVideo *dest, VisVideo *src, int alpha)
{
	visual_return_val_if_fail (dest != NULL, NULL);
	visual_return_val_if_fail (src  != NULL, NULL);

	switch (src->compose_type) {
		case VISUAL_VIDEO_COMPOSE_TYPE_NONE:
			return blit_overlay_noalpha;

		case VISUAL_VIDEO_COMPOSE_TYPE_SRC:
			if (!alpha || src->depth != VISUAL_VIDEO_DEPTH_32BIT)
				return blit_overlay_noalpha;
			else
				return blit_overlay_alphasrc;

		case VISUAL_VIDEO_COMPOSE_TYPE_COLORKEY:
			return blit_overlay_colorkey;

		case VISUAL_VIDEO_COMPOSE_TYPE_SURFACE:
			return blit_overlay_surfacealpha;

		case VISUAL_VIDEO_COMPOSE_TYPE_SURFACECOLORKEY:
			return blit_overlay_surfacealphacolorkey;

		case VISUAL_VIDEO_COMPOSE_TYPE_CUSTOM:
			return src->compose_func;

		default:
			return NULL;
	}
}

void visual_video_blit_area (VisVideo     *dest,
                             VisRectangle *drect,
                             VisVideo     *src,
                             VisRectangle *srect,
                             int           alpha)
{
    VisVideoComposeFunc func = visual_video_get_compose_function (dest, src, alpha);

	visual_video_compose_area (dest, drect, src, srect, func);
}

void visual_video_compose_area (VisVideo            *dest,
                                VisRectangle        *drect,
                                VisVideo            *src,
                                VisRectangle        *srect,
                                VisVideoComposeFunc  compose_func)
{
	VisVideo *vsrc = NULL;
	VisRectangle *ndrect = NULL;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	visual_return_if_fail (drect != NULL);
	visual_return_if_fail (srect != NULL);

	vsrc = visual_video_new ();

	ndrect = visual_rectangle_clone (drect);
	visual_rectangle_normalize_to (ndrect, srect);

	visual_video_region_sub_with_boundary (vsrc, ndrect, src, srect);

	visual_video_compose (dest, vsrc,
                          visual_rectangle_get_x (drect),
                          visual_rectangle_get_y (drect),
                          compose_func);

	visual_rectangle_free (ndrect);
	visual_object_unref (VISUAL_OBJECT (vsrc));
}

void visual_video_blit_scale_area (VisVideo           *dest,
                                   VisRectangle       *drect,
                                   VisVideo           *src,
                                   VisRectangle       *srect,
                                   int                 alpha,
                                   VisVideoScaleMethod scale_method)
{
    VisVideoComposeFunc func = visual_video_get_compose_function (dest, src, alpha);

	return visual_video_compose_scale_area (dest, drect, src, srect, scale_method, func);
}

void visual_video_compose_scale_area (VisVideo            *dest,
                                      VisRectangle        *drect,
                                      VisVideo            *src,
                                      VisRectangle        *srect,
                                      VisVideoScaleMethod  scale_method,
                                      VisVideoComposeFunc  compose_func)
{
	VisVideo *svid = NULL;
	VisVideo *ssrc = NULL;
	VisRectangle *frect = NULL;
	VisRectangle *sbound = NULL;

	visual_return_if_fail (dest  != NULL);
	visual_return_if_fail (src   != NULL);
	visual_return_if_fail (drect != NULL);
	visual_return_if_fail (srect != NULL);

	svid = visual_video_new ();
	ssrc = visual_video_new ();

	sbound = visual_video_get_extents (dest);

	/* check if the rectangle is in the screen, if not, don't scale and such */
	if (!visual_rectangle_intersects (sbound, drect)) {
		/* FIXME: set error here */
		goto out;
	}

	visual_video_region_sub (ssrc, src, srect);

	visual_video_set_attributes (svid,
	                             visual_rectangle_get_width (drect),
	                             visual_rectangle_get_height (drect),
	                             src->bpp * visual_rectangle_get_width (drect),
	                             src->depth);

	visual_video_allocate_buffer (svid);

	/* Scale the source to the dest rectangle it's size */
	visual_video_scale (svid, ssrc, scale_method);

	frect = visual_rectangle_clone (drect);
	visual_rectangle_normalize (frect);

	/* Blit the scaled source into the dest rectangle */
	visual_video_compose_area (dest, drect, svid, frect, compose_func);

out:
	if (frect)  visual_rectangle_free (frect);
	if (sbound) visual_rectangle_free (sbound);

	if (svid)   visual_object_unref (VISUAL_OBJECT (svid));
	if (ssrc)   visual_object_unref (VISUAL_OBJECT (ssrc));
}

void visual_video_blit (VisVideo *dest, VisVideo *src, int x, int y, int alpha)
{
    VisVideoComposeFunc func = visual_video_get_compose_function (dest, src, alpha);

    return visual_video_compose (dest, src, x, y, func);
}

void visual_video_compose (VisVideo *dest, VisVideo *src, int x, int y, VisVideoComposeFunc compose_func)
{
	VisVideo *transform = NULL;
	VisVideo *srcp = NULL;
	VisVideo *dregion = NULL;
	VisVideo *sregion = NULL;
	VisVideo *tempregion = NULL;
	VisRectangle *redestrect = NULL;
	VisRectangle *drect = NULL;
	VisRectangle *srect = NULL;
	VisRectangle *trect = NULL;

	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);
	visual_return_if_fail (compose_func != NULL);

	/* We can't overlay GL surfaces so don't even try */
	visual_return_if_fail (dest->depth != VISUAL_VIDEO_DEPTH_GL);
    visual_return_if_fail (src->depth  != VISUAL_VIDEO_DEPTH_GL);

	drect = visual_video_get_extents (dest);
	srect = visual_video_get_extents (src);

	if (!visual_rectangle_intersects (drect, srect))
		goto out;

	/* We're not the same depth, converting */
	if (dest->depth != src->depth) {
		transform = visual_video_new ();

		visual_video_set_depth (transform, dest->depth);
		visual_video_set_dimension (transform, src->width, src->height);

		visual_video_allocate_buffer (transform);

		visual_video_convert_depth (transform, src);
	}

	/* Setting all the pointers right */
	srcp = transform ? transform : src;

	dregion = visual_video_new ();
	sregion = visual_video_new ();
	tempregion = visual_video_new ();

	/* Negative offset fixture */
	if (x < 0) {
		visual_rectangle_set_x (srect, visual_rectangle_get_x (srect) - x);
		visual_rectangle_set_width (srect, visual_rectangle_get_width (srect) + x);
		x = 0;
	}

	if (y < 0) {
		visual_rectangle_set_y (srect, visual_rectangle_get_y (srect) - y);
		visual_rectangle_set_height (srect, visual_rectangle_get_height (srect) + y);
		y = 0;
	}

	/* Retrieve sub regions */
	trect = visual_rectangle_new (x, y, visual_rectangle_get_width (srect), visual_rectangle_get_height (srect));

	visual_video_region_sub_with_boundary (dregion, drect, dest, trect);

	redestrect = visual_video_get_extents (dregion);

	visual_video_region_sub (tempregion, srcp, srect);
	visual_video_region_sub_with_boundary (sregion, drect, tempregion, redestrect);

	/* Call blitter */
	compose_func (dregion, sregion);

out:
	if (redestrect) visual_rectangle_free (redestrect);
	if (drect)      visual_rectangle_free (drect);
	if (srect)      visual_rectangle_free (srect);
	if (trect)      visual_rectangle_free (trect);

	/* If we had a transform buffer, it's time to get rid of it */
	if (transform)  visual_object_unref (VISUAL_OBJECT (transform));
	if (dregion)    visual_object_unref (VISUAL_OBJECT (dregion));
	if (sregion)    visual_object_unref (VISUAL_OBJECT (sregion));
	if (tempregion)	visual_object_unref (VISUAL_OBJECT (tempregion));
}


void visual_video_fill_alpha_color (VisVideo *video, VisColor *color, uint8_t alpha)
{
	int x, y;
	int col = 0;
	uint32_t *vidbuf;

	visual_return_if_fail (video != NULL);
	visual_return_if_fail (video->depth == VISUAL_VIDEO_DEPTH_32BIT);

	col = (color->r << 16 | color->g << 8 | color->b);

	vidbuf = visual_video_get_pixels (video);

	/* FIXME byte order sensitive */
	for (y = 0; y < video->height; y++) {
		for (x = 0; x < video->width; x++) {
			if ((*vidbuf & 0x00ffffff) == col)
				*vidbuf = col;
			else
				*vidbuf |= alpha << 24;

			vidbuf++;
		}

		vidbuf += video->pitch - (video->width * video->bpp);
	}
}

void visual_video_fill_alpha (VisVideo *video, uint8_t alpha)
{
	int x, y;
	uint8_t *vidbuf;

	visual_return_if_fail (video != NULL);
	visual_return_if_fail (video->depth == VISUAL_VIDEO_DEPTH_32BIT);

	vidbuf = (uint8_t *) visual_video_get_pixels (video) + 3;

	/* FIXME byte order sensitive */
	for (y = 0; y < video->height; y++) {
		for (x = 0; x < video->width; x++)
			*(vidbuf += video->bpp) = alpha;

		vidbuf += video->pitch - (video->width * video->bpp);
	}
}

void visual_video_fill_alpha_area (VisVideo *video, uint8_t alpha, VisRectangle *area)
{
	VisVideo *rvid = NULL;

	visual_return_if_fail (video != NULL);
	visual_return_if_fail (area  != NULL);
	visual_return_if_fail (video->depth == VISUAL_VIDEO_DEPTH_32BIT);

	rvid = visual_video_new ();

	visual_video_region_sub (video, rvid, area);

	visual_video_fill_alpha (rvid, alpha);

	visual_object_unref (VISUAL_OBJECT (rvid));
}

void visual_video_fill_color (VisVideo *video, VisColor *rcolor)
{
	VisColor color;

	visual_return_if_fail (video != NULL);

	if (!rcolor)
		visual_color_set (&color, 0, 0, 0);
	else
		visual_color_copy (&color, rcolor);

	switch (video->depth) {
		case VISUAL_VIDEO_DEPTH_8BIT:
			visual_video_fill_color_index8 (video, &color);
            return;

		case VISUAL_VIDEO_DEPTH_16BIT:
			visual_video_fill_color_rgb16 (video, &color);
            return;

		case VISUAL_VIDEO_DEPTH_24BIT:
			visual_video_fill_color_rgb24 (video, &color);
			return;

		case VISUAL_VIDEO_DEPTH_32BIT:
			visual_video_fill_color_argb32 (video, &color);
			return;

		default:
			return;
	}
}

void visual_video_fill_color_area (VisVideo *video, VisColor *color, VisRectangle *area)
{
	VisRectangle *vrect  = NULL;
	VisRectangle *dbound = NULL;
	VisVideo *svid = NULL;

	visual_return_if_fail (video != NULL);
	visual_return_if_fail (color != NULL);
	visual_return_if_fail (area  != NULL);

	vrect = visual_video_get_extents (video);

	if (!visual_rectangle_intersects (vrect, area))
		goto out;

	svid = visual_video_new ();

	dbound = visual_video_get_extents (video);

	visual_video_region_sub_with_boundary (svid, dbound, video, area);

	visual_video_fill_color (svid, color);

out:

	if (dbound) visual_rectangle_free (dbound);
	if (vrect)  visual_rectangle_free (vrect);
	if (svid)   visual_object_unref (VISUAL_OBJECT (svid));
}


void visual_video_flip_pixel_bytes (VisVideo *dest, VisVideo *src)
{
	visual_return_if_fail (visual_video_compare_attrs (dest, src));
	visual_return_if_fail (visual_video_get_pixels (dest) != NULL);
	visual_return_if_fail (visual_video_get_pixels (src)  != NULL);

	switch (dest->depth) {
		case VISUAL_VIDEO_DEPTH_16BIT:
			visual_video_flip_pixel_bytes_color16 (dest, src);
			return;

		case VISUAL_VIDEO_DEPTH_24BIT:
			visual_video_flip_pixel_bytes_color24 (dest, src);
			return;

		case VISUAL_VIDEO_DEPTH_32BIT:
			visual_video_flip_pixel_bytes_color32 (dest, src);
			return;

		default:
			return;
	}
}

void visual_video_rotate (VisVideo *dest, VisVideo *src, VisVideoRotateDegrees degrees)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	switch (degrees) {
		case VISUAL_VIDEO_ROTATE_NONE:
			if (dest->width == src->width && dest->height == src->height)
				visual_video_blit (dest, src, 0, 0, FALSE);
            return;

		case VISUAL_VIDEO_ROTATE_90:
			rotate_90 (dest, src);
            return;

		case VISUAL_VIDEO_ROTATE_180:
			rotate_180 (dest, src);
            return;

		case VISUAL_VIDEO_ROTATE_270:
			rotate_270 (dest, src);
            return;

		default:
			return;
	}
}

/* rotate functions, works with all depths now */
/* FIXME: do more testing with those badasses */
static void rotate_90 (VisVideo *dest, VisVideo *src)
{
	int x, y, i;

	uint8_t *tsbuf = src->pixel_rows[src->height-1];
	uint8_t *dbuf;
	uint8_t *sbuf = tsbuf;

	visual_return_if_fail (dest->width == src->height);
	visual_return_if_fail (dest->height == src->width);

	for (y = 0; y < dest->height; y++) {
		dbuf = dest->pixel_rows[y];

		for (x = 0; x < dest->width; x++) {
			for (i = 0; i < dest->bpp; i++) {
				*(dbuf++) = *(sbuf + i);
			}

			sbuf -= src->pitch;
		}

		tsbuf += src->bpp;
		sbuf = tsbuf;
	}
}

static void rotate_180 (VisVideo *dest, VisVideo *src)
{
	int x, y, i;

	uint8_t *dbuf;
	uint8_t *sbuf;

	const int h1 = src->height - 1;
	const int w1 = (src->width - 1) * src->bpp;

	visual_return_if_fail (dest->width  == src->width);
	visual_return_if_fail (dest->height == src->height);

	for (y = 0; y < dest->height; y++) {
		dbuf = dest->pixel_rows[y];
		sbuf = (uint8_t *) src->pixel_rows[h1 - y] + w1;

		for (x = 0; x < dest->width; x++) {
			for (i = 0; i < src->bpp; i++) {
				*(dbuf++) = *(sbuf + i);
			}

			sbuf -= src->bpp;
		}
	}
}

static void rotate_270 (VisVideo *dest, VisVideo *src)
{
	int x, y, i;

	uint8_t *tsbuf = (uint8_t *) visual_video_get_pixels (src) + src->pitch - src->bpp;
	uint8_t *dbuf = visual_video_get_pixels (dest);
	uint8_t *sbuf = tsbuf;

	visual_return_if_fail (dest->width == src->height);
	visual_return_if_fail (dest->height == src->width);

	for (y = 0; y < dest->height; y++) {
		dbuf = dest->pixel_rows[y];

		for (x = 0; x < dest->width; x++) {
			for (i = 0; i < dest->bpp; i++) {
				*(dbuf++) = *(sbuf + i);
			}

			sbuf += src->pitch;
		}

		tsbuf -= src->bpp;
		sbuf = tsbuf;
	}
}

void visual_video_mirror (VisVideo *dest, VisVideo *src, VisVideoMirrorOrient orient)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);
	visual_return_if_fail (src->depth == dest->depth);

	switch (orient) {
		case VISUAL_VIDEO_MIRROR_NONE: visual_video_blit (dest, src, 0, 0, FALSE); return;
		case VISUAL_VIDEO_MIRROR_X   : mirror_x (dest, src); return;
		case VISUAL_VIDEO_MIRROR_Y   : mirror_y (dest, src); return;

		default:
			return;
	}
}

/* Mirror functions */
static void mirror_x (VisVideo *dest, VisVideo *src)
{
	uint8_t *dbuf = visual_video_get_pixels (dest);
	uint8_t *sbuf = visual_video_get_pixels (src);
	const int step2 = dest->bpp << 1;
	const int w1b = (dest->width - 1) * dest->bpp;
	int x, y, i;

	for (y = 0; y < dest->height; y++) {
		sbuf = (uint8_t *) src->pixel_rows[y] + w1b;
		dbuf = dest->pixel_rows[y];

		for (x = 0; x < dest->width; x++) {

			for (i = 0; i < dest->bpp; i++)
				*(dbuf++) = *(sbuf++);

			sbuf -= step2;
		}
	}
}

static void mirror_y (VisVideo *dest, VisVideo *src)
{
	int y;

	for (y = 0; y < dest->height; y++) {
		visual_mem_copy (dest->pixel_rows[y],
				src->pixel_rows[dest->height - 1 - y],
				dest->width * dest->bpp);
	}
}

void visual_video_convert_depth (VisVideo *dest, VisVideo *src)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	/* We blit overlay it instead of just visual_mem_copy because the pitch can still be different */
	if (dest->depth == src->depth) {
		visual_video_blit (dest, src, 0, 0, FALSE);
        return;
    }

	if (dest->depth == VISUAL_VIDEO_DEPTH_8BIT || src->depth == VISUAL_VIDEO_DEPTH_8BIT) {
		visual_return_if_fail (src->pal != NULL);
		visual_return_if_fail (visual_palette_get_size (src->pal) == 256);
	}

	if (src->depth == VISUAL_VIDEO_DEPTH_8BIT) {

	    if (dest->depth == VISUAL_VIDEO_DEPTH_16BIT) {
			visual_video_index8_to_rgb16 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_24BIT) {
			visual_video_index8_to_rgb24 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_32BIT) {
			visual_video_index8_to_argb32 (dest, src);
			return;
		}

	} else if (src->depth == VISUAL_VIDEO_DEPTH_16BIT) {

		if (dest->depth == VISUAL_VIDEO_DEPTH_8BIT) {
			visual_video_rgb16_to_index8 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_24BIT) {
			visual_video_rgb16_to_rgb24 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_32BIT) {
			visual_video_rgb16_to_argb32 (dest, src);
			return;
		}

	} else if (src->depth == VISUAL_VIDEO_DEPTH_24BIT) {

		if (dest->depth == VISUAL_VIDEO_DEPTH_8BIT) {
			visual_video_rgb24_to_index8 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_16BIT) {
			visual_video_rgb24_to_rgb16 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_32BIT) {
			visual_video_rgb24_to_argb32 (dest, src);
			return;
		}

	} else if (src->depth == VISUAL_VIDEO_DEPTH_32BIT) {

		if (dest->depth == VISUAL_VIDEO_DEPTH_8BIT) {
			visual_video_argb32_to_index8 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_16BIT) {
			visual_video_argb32_to_rgb16 (dest, src);
			return;
		}

		if (dest->depth == VISUAL_VIDEO_DEPTH_24BIT) {
			visual_video_argb32_to_rgb24 (dest, src);
			return;
		}
	}
}

static inline int is_valid_scale_method (VisVideoScaleMethod scale_method)
{
    return scale_method == VISUAL_VIDEO_SCALE_NEAREST
	    || scale_method == VISUAL_VIDEO_SCALE_BILINEAR;
}

void visual_video_scale (VisVideo *dest, VisVideo *src, VisVideoScaleMethod method)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);
	visual_return_if_fail (dest->depth == src->depth);
	visual_return_if_fail (is_valid_scale_method (method));

	/* If the dest and source are equal in dimension and scale_method is nearest, do a
	 * blit overlay */
	if (visual_video_compare_attrs_ignore_pitch (dest, src) && method == VISUAL_VIDEO_SCALE_NEAREST) {
		visual_video_blit (dest, src, 0, 0, FALSE);
		return;
	}

	switch (dest->depth) {
		case VISUAL_VIDEO_DEPTH_8BIT:
			if (method == VISUAL_VIDEO_SCALE_NEAREST)
				visual_video_scale_nearest_color8 (dest, src);
			else if (method == VISUAL_VIDEO_SCALE_BILINEAR)
				visual_video_scale_bilinear_color8 (dest, src);

			break;

		case VISUAL_VIDEO_DEPTH_16BIT:
			if (method == VISUAL_VIDEO_SCALE_NEAREST)
				visual_video_scale_nearest_color16 (dest, src);
			else if (method == VISUAL_VIDEO_SCALE_BILINEAR)
				visual_video_scale_bilinear_color16 (dest, src);

			break;

		case VISUAL_VIDEO_DEPTH_24BIT:
			if (method == VISUAL_VIDEO_SCALE_NEAREST)
				visual_video_scale_nearest_color24 (dest, src);
			else if (method == VISUAL_VIDEO_SCALE_BILINEAR)
				visual_video_scale_bilinear_color24 (dest, src);

			break;

		case VISUAL_VIDEO_DEPTH_32BIT:
			if (method == VISUAL_VIDEO_SCALE_NEAREST)
				visual_video_scale_nearest_color32 (dest, src);
			else if (method == VISUAL_VIDEO_SCALE_BILINEAR) {
				if (visual_cpu_has_mmx ())
					_lv_scale_bilinear_32_mmx (dest, src);
				else
					visual_video_scale_bilinear_color32 (dest, src);
			}

			break;

		default:
			visual_log (VISUAL_LOG_ERROR, _("Invalid depth passed to the scaler"));
			break;
	}
}

void visual_video_scale_depth (VisVideo *dest, VisVideo *src, VisVideoScaleMethod scale_method)
{
	visual_return_if_fail (dest != NULL);
	visual_return_if_fail (src  != NULL);

	if (dest->depth != src->depth) {
		VisVideo *dtransform = NULL;

		dtransform = visual_video_new ();

		visual_video_set_attributes (dtransform, dest->width, dest->height, dest->width * dest->bpp, dest->depth);
		visual_video_allocate_buffer (dtransform);

		visual_video_convert_depth (dtransform, src);

		visual_video_scale (dest, dtransform, scale_method);

		visual_object_unref (VISUAL_OBJECT (dtransform));
	} else {
		visual_video_scale (dest, src, scale_method);
	}
}

/* VisVideoDepth functions */

int visual_video_depth_is_supported (int depthflag, VisVideoDepth depth)
{
	if (visual_video_depth_is_sane (depth) == 0)
		return -VISUAL_ERROR_VIDEO_INVALID_DEPTH;

	if ((depth & depthflag) > 0)
		return TRUE;

	return FALSE;
}

VisVideoDepth visual_video_depth_get_next (int depthflag, VisVideoDepth depth)
{
	int i = depth;

	if (visual_video_depth_is_sane (depth) == 0)
		return VISUAL_VIDEO_DEPTH_ERROR;

	if (i == VISUAL_VIDEO_DEPTH_NONE) {
		i = VISUAL_VIDEO_DEPTH_8BIT;

		if ((i & depthflag) > 0)
			return i;
	}

	while (i < VISUAL_VIDEO_DEPTH_GL) {
		i *= 2;

		if ((i & depthflag) > 0)
			return i;
	}

	return depth;
}

VisVideoDepth visual_video_depth_get_prev (int depthflag, VisVideoDepth depth)
{
	int i = depth;

	if (visual_video_depth_is_sane (depth) == 0)
		return VISUAL_VIDEO_DEPTH_ERROR;

	if (i == VISUAL_VIDEO_DEPTH_NONE)
		return VISUAL_VIDEO_DEPTH_NONE;

	while (i > VISUAL_VIDEO_DEPTH_NONE) {
		i >>= 1;

		if ((i & depthflag) > 0)
			return i;
	}

	return depth;
}

VisVideoDepth visual_video_depth_get_lowest (int depthflag)
{
	return visual_video_depth_get_next (depthflag, VISUAL_VIDEO_DEPTH_NONE);
}

VisVideoDepth visual_video_depth_get_highest (int depthflag)
{
	VisVideoDepth highest = VISUAL_VIDEO_DEPTH_NONE;
	VisVideoDepth i = 0;
	int firstentry = TRUE;

	while (highest != i || firstentry) {
		highest = i;

		i = visual_video_depth_get_next (depthflag, i);

		firstentry = FALSE;
	}

	return highest;
}

VisVideoDepth visual_video_depth_get_highest_nogl (int depthflag)
{
	VisVideoDepth depth;

	depth = visual_video_depth_get_highest (depthflag);

	/* Get previous depth if the highest is openGL */
	if (depth == VISUAL_VIDEO_DEPTH_GL) {
		depth = visual_video_depth_get_prev (depthflag, depth);

		/* Is it still on openGL ? Return an error */
		if (depth == VISUAL_VIDEO_DEPTH_GL)
			return VISUAL_VIDEO_DEPTH_ERROR;

	} else {
		return depth;
	}

	return -VISUAL_ERROR_IMPOSSIBLE;
}

int visual_video_depth_is_sane (VisVideoDepth depth)
{
	int count = 0;
	int i = 1;

	if (depth == VISUAL_VIDEO_DEPTH_NONE)
		return TRUE;

	if (depth >= VISUAL_VIDEO_DEPTH_ENDLIST)
		return FALSE;

	while (i < VISUAL_VIDEO_DEPTH_ENDLIST) {
		if ((i & depth) > 0)
			count++;

		if (count > 1)
			return FALSE;

		i <<= 1;
	}

	return TRUE;
}

int visual_video_depth_value_from_enum (VisVideoDepth depth)
{
	switch (depth) {
		case VISUAL_VIDEO_DEPTH_8BIT:
			return 8;

		case VISUAL_VIDEO_DEPTH_16BIT:
			return 16;

		case VISUAL_VIDEO_DEPTH_24BIT:
			return 24;

		case VISUAL_VIDEO_DEPTH_32BIT:
			return 32;

		default:
			return -VISUAL_ERROR_VIDEO_INVALID_DEPTH;
	}

	return -VISUAL_ERROR_VIDEO_INVALID_DEPTH;
}

VisVideoDepth visual_video_depth_enum_from_value (int depthvalue)
{
	switch (depthvalue) {
		case  8: return VISUAL_VIDEO_DEPTH_8BIT;
		case 16: return VISUAL_VIDEO_DEPTH_16BIT;
		case 24: return VISUAL_VIDEO_DEPTH_24BIT;
		case 32: return VISUAL_VIDEO_DEPTH_32BIT;

		default:
            return VISUAL_VIDEO_DEPTH_ERROR;
	}

	return -VISUAL_ERROR_IMPOSSIBLE;
}

int visual_video_bpp_from_depth (VisVideoDepth depth)
{
	switch (depth) {
		case VISUAL_VIDEO_DEPTH_8BIT:  return 1;
		case VISUAL_VIDEO_DEPTH_16BIT: return 2;
		case VISUAL_VIDEO_DEPTH_24BIT: return 3;
		case VISUAL_VIDEO_DEPTH_32BIT: return 4;
		case VISUAL_VIDEO_DEPTH_GL:    return 0;

		default:
			return -VISUAL_ERROR_VIDEO_INVALID_DEPTH;
	}

	return -VISUAL_ERROR_IMPOSSIBLE;
}
