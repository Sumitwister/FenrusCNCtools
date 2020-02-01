/*
 * (C) Copyright 2019  -  Arjan van de Ven <arjanvandeven@gmail.com>
 *
 * This file is part of FenrusCNCtools
 *
 * SPDX-License-Identifier: GPL-3.0
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

extern "C" {
#include <math.h>
#include "fenrus.h"
#include "toolpath.h"
}
#include "scene.h"

struct stltriangle {
	float normal[3];
	float vertex1[3];
	float vertex2[3];
	float vertex3[3];
	uint16_t attribute;
} __attribute__((packed));

static int toolnr;
static double tooldepth = 0.1;


static inline double dist(double X0, double Y0, double X1, double Y1)
{
  return sqrt((X1-X0)*(X1-X0) + (Y1-Y0)*(Y1-Y0));
}

static int read_stl_file(const char *filename)
{
	FILE *file;
	char header[80];
	uint32_t trianglecount;
	int ret;
	uint32_t i;

	file = fopen(filename, "rb");
	if (!file) {
		printf("Failed to open file %s: %s\n", filename, strerror(errno));
		return -1;
	}
	
	ret = fread(header, 1, 80, file);
	if (ret <= 0) {
		printf("STL file too short\n");
		fclose(file);
		return -1;
	}
	ret = fread(&trianglecount, 1, 4, file);
	set_max_triangles(trianglecount);

	for (i = 0; i < trianglecount; i++) {
		struct stltriangle t;
		ret = fread(&t, 1, sizeof(struct stltriangle), file);
		if (ret < 1)
			break;
		push_triangle(t.vertex1, t.vertex2, t.vertex3);
	}

	fclose(file);
	return 0;
}

static double last_X,  last_Y, last_Z;
static double cur_X, cur_Y, cur_Z;
static bool first;

static void line_to(class inputshape *input, double X2, double Y2, double Z2)
{
	double X1 = last_X, Y1 = last_Y, Z1 = last_Z;
	unsigned int depth = 0;

	last_X = X2;
	last_Y = Y2;
	last_Z = Z2;

	if (first) {
		first = false;
		cur_X = X2;
		cur_Y = Y2;
		cur_Z = Z2;
		return;
	}

	while (Z1 < -0.000001 || Z2 < -0.00001) {
//		printf("at depth %i    %5.2f %5.2f %5.2f -> %5.2f %5.2f %5.2f\n", depth, X1, Y1, Z1, X2, Y2, Z2);
		depth++;
		while (input->tooldepths.size() <= depth) {
				class tooldepth * td = new(class tooldepth);
				td->depth = Z1;
				td->toolnr = toolnr;
				td->diameter = get_tool_diameter();
				input->tooldepths.push_back(td);				
		}

		if (input->tooldepths[depth]->toollevels.size() < 1) {
					class toollevel *tool = new(class toollevel);
					tool->level = 0;
					tool->offset = get_tool_diameter();
					tool->diameter = get_tool_diameter();
					tool->depth = Z1;
					tool->toolnr = toolnr;
					tool->minY = 0;
					tool->name = "Manual toolpath";
					tool->no_sort = true;
					input->tooldepths[depth]->toollevels.push_back(tool);
		}
		Polygon_2 *p2;
		p2 = new(Polygon_2);
		p2->push_back(Point(X1, Y1));
		p2->push_back(Point(X2, Y2));
		input->tooldepths[depth]->toollevels[0]->add_poly_vcarve(p2, Z1, Z2);
		
		Z1 += tooldepth;
		Z2 += tooldepth;

		Z1 = ceil(Z1 * 20) / 20.0;
		Z2 = ceil(Z2 * 20) / 20.0;
	}
}

/*
sin/cos circle table:
Angle  0.00     X 1.0000   Y 0.0000
Angle 22.50     X 0.9239   Y 0.3827
Angle 45.00     X 0.7071   Y 0.7071
Angle 67.50     X 0.3827   Y 0.9239
Angle 90.00     X 0.0000   Y 1.0000
Angle 112.50     X -0.3827   Y 0.9239
Angle 135.00     X -0.7071   Y 0.7071
Angle 157.50     X -0.9239   Y 0.3827
Angle 180.00     X -1.0000   Y 0.0000
Angle 202.50     X -0.9239   Y -0.3827
Angle 225.00     X -0.7071   Y -0.7071
Angle 247.50     X -0.3827   Y -0.9239
Angle 270.00     X -0.0000   Y -1.0000
Angle 292.50     X 0.3827   Y -0.9239
Angle 315.00     X 0.7071   Y -0.7071
Angle 337.50     X 0.9239   Y -0.3827
*/

#define ACC 100.0

static inline double get_height_tool(double X, double Y, double R, bool ballnose)
{	
	double d = 0, dorg;
	double orgR = R;
	double balloffset = 0.0;

	d = fmax(d, get_height(X + 0.0000 * R, Y + 0.0000 * R));

	if (ballnose) {
		balloffset = sqrt(orgR*orgR - R*R) - orgR;
	}

	d = fmax(d, get_height(X + 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X + 0.0000 * R, Y + 1.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 0.0000 * R, Y - 1.0000 * R) + balloffset);

	dorg = d;
	d = fmax(d, get_height(X + 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y - 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X + 0.7071 * R, Y - 0.7071 * R) + balloffset);

	if (R < 0.6 && fabs(d-dorg) < 0.1)
		return ceil(d*ACC)/ACC;

	d = fmax(d, get_height(X + 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3872 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y - 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.9239 * R, Y - 0.3827 * R) + balloffset);

	R = R / 1.5;

	if (R < 0.4)
		return ceil(d*ACC)/ACC;

	if (ballnose) {
		balloffset = sqrt(orgR*orgR - R*R) - orgR;
	}

	d = fmax(d, get_height(X + 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X + 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X + 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.0000 * R, Y + 1.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3872 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y - 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y - 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.0000 * R, Y - 1.0000 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.7071 * R, Y - 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X + 0.9239 * R, Y - 0.3827 * R) + balloffset);

	R = R / 1.5;

	if (R < 0.4)
		return ceil(d*ACC)/ACC;


	if (ballnose)
		balloffset = sqrt(orgR*orgR - R*R) - orgR;

	d = fmax(d, get_height(X + 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X + 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X + 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.0000 * R, Y + 1.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3872 * R, Y + 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y + 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y + 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 1.0000 * R, Y + 0.0000 * R) + balloffset);
	d = fmax(d, get_height(X - 0.9239 * R, Y - 0.3827 * R) + balloffset);
	d = fmax(d, get_height(X - 0.7071 * R, Y - 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X - 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X - 0.0000 * R, Y - 1.0000 * R) + balloffset);
	d = fmax(d, get_height(X + 0.3827 * R, Y - 0.9239 * R) + balloffset);
	d = fmax(d, get_height(X + 0.7071 * R, Y - 0.7071 * R) + balloffset);
	d = fmax(d, get_height(X + 0.9239 * R, Y - 0.3827 * R) + balloffset);


	return ceil(d*ACC)/ACC;

}

static void print_progress(double pct) {
	char line[] = "----------------------------------------";
	int i;
	int len = strlen(line);
	for (i = 0; i < len; i++ ) {
		if (i * 100.0 / len < pct)
			line[i] = '#';
	}
	printf("Progress =[%s]=     \r", line);
	fflush(stdout);
}

static void create_cutout(class scene *scene, int tool)
{
	Polygon_2 *p;
	double diam = tool_diam(tool);;
	double gradient = 0, circumfence = 0;
	double currentdepth = -scene->get_cutout_depth();
	class inputshape *input;

	input = new(class inputshape);
	input->set_name("Cutout path");
	scene->shapes.push_back(input);

	p = new(Polygon_2);
	p->push_back(Point(-diam/2, -diam/2));
	p->push_back(Point(stl_image_X() + diam/2, -diam/2));
	p->push_back(Point(stl_image_X() + diam/2, stl_image_Y() + diam/2));
	p->push_back(Point(-diam/2, stl_image_Y() + diam / 2));

	for (unsigned int i = 0; i < p->size(); i++) {
		unsigned int next = i + 1;
		if (next >= p->size())
			next = 0;

		class tooldepth * td = new(class tooldepth);
		input->tooldepths.push_back(td);
		td->depth = currentdepth;
		td->toolnr = toolnr;
		td->diameter = get_tool_diameter();
		
		class toollevel *tool = new(class toollevel);
		tool->level = 0;
		tool->offset = get_tool_diameter();
		tool->diameter = get_tool_diameter();
		tool->depth = currentdepth;
		tool->toolnr = toolnr;
		tool->minY = 0;
		tool->name = "Cutout";
		td->toollevels.push_back(tool);

	    Polygon_2 *p2;
		p2 = new(Polygon_2);
		double d1 = currentdepth;
		p2->push_back(Point(CGAL::to_double((*p)[i].x()), CGAL::to_double((*p)[i].y())));
		p2->push_back(Point(CGAL::to_double((*p)[next].x()), CGAL::to_double((*p)[next].y())));
		tool->add_poly_vcarve(p2, d1, d1);
	}			

	for (unsigned int i = 0; i < p->size(); i++) {
		unsigned int next = i + 1;
		if (next >= p->size())
			next = 0;
		circumfence += dist(
					CGAL::to_double((*p)[i].x()), 
					CGAL::to_double((*p)[i].y()), 
					CGAL::to_double((*p)[next].x()), 
					CGAL::to_double((*p)[next].y()));
	}

	if (circumfence == 0)
		return;

	gradient = fabs(get_tool_maxdepth()) / circumfence;
	/*     walk the gradient up until we break the surface */
	while (currentdepth < 0) {
		for (unsigned int i = 0; i < p->size(); i++) {
			unsigned int next = i + 1;
			if (next >= p->size())
				next = 0;

			if (currentdepth > 0)
				break;

			class tooldepth * td = new(class tooldepth);
			input->tooldepths.push_back(td);
			td->depth = currentdepth;
			td->toolnr = toolnr;
			td->diameter = get_tool_diameter();
			class toollevel *tool = new(class toollevel);
			tool->level = 0;
			tool->offset = get_tool_diameter();
			tool->diameter = get_tool_diameter();
			tool->depth = currentdepth;
			tool->toolnr = toolnr;
			tool->minY = 0;
			tool->name = NULL;
			td->toollevels.push_back(tool);

			Polygon_2 *p2;
			p2 = new(Polygon_2);
			double d1 = currentdepth;
			double d2 = gradient * dist(	CGAL::to_double((*p)[i].x()), 
														CGAL::to_double((*p)[i].y()), 
														CGAL::to_double((*p)[next].x()), 
														CGAL::to_double((*p)[next].y()));
			p2->push_back(Point(CGAL::to_double((*p)[i].x()), CGAL::to_double((*p)[i].y())));
			p2->push_back(Point(CGAL::to_double((*p)[next].x()), CGAL::to_double((*p)[next].y())));
			tool->add_poly_vcarve(p2, d1, d1 + d2);
			currentdepth += d2;
					
		}
	}
}

static bool outside_area(double X, double Y, double mX, double mY, double diam)
{
	if (Y > mY)
		Y = mY - Y;

	if (X > mX)
		X = mX - X;

	if (X < 0 && Y < 0 && sqrt(X*X+Y*Y) > diam/2 * 0.90)
		return true;

	return false;

}

static void create_toolpath(class scene *scene, int tool, bool roughing, bool has_cutout, bool even)
{
	double X, Y = 0, maxX, maxY, stepover;
	double maxZ, diam, radius;
	double offset = 0;
	bool ballnose = false;

	double overshoot;

	class inputshape *input;
	toolnr = tool;
	diam = tool_diam(tool);
	maxZ = scene->get_cutout_depth();


	radius = diam / 2;
	/* when roughing, look much wider */
	if (roughing)
		radius = diam;

	overshoot = diam/2 * 0.9;

	if (!has_cutout) {
		overshoot = overshoot /2;
		if (roughing)
			overshoot = 0;
	}

	Y = -overshoot;
	maxX = stl_image_X() + overshoot;
	maxY = stl_image_Y() + overshoot;

	stepover = get_tool_stepover(toolnr);

	if (!roughing && stepover > 0.2)
		stepover = stepover / 1.42;

	if (!roughing && tool_is_ballnose(tool)) { 
		stepover = stepover / 2;
		ballnose = true;
		if (scene->get_finishing_pass_stepover() > 0)
			stepover = scene->get_finishing_pass_stepover();
	}

	offset = 0;
	if (roughing) 
		offset = scene->get_stock_to_leave();

	if (roughing)
		gcode_set_roughing(1);

	if (even) {
		input = new(class inputshape);
		input->set_name("STL path");
		scene->shapes.push_back(input);
		first = true;
		while (Y < maxY) {
			double prevX;
			X = -overshoot;
			prevX = X;
			while (X < maxX) {
				double d;
				d = get_height_tool(X, Y, radius + offset, ballnose);

				if (fabs(d - last_Z) > 1 && roughing) {
					X = prevX + stepover / 1.5;
					d = get_height_tool(X, Y, radius + offset, ballnose);
				}

				if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
					line_to(input, X, Y, -maxZ + d + offset);

				prevX = X;
				X = X + stepover;
			}
			print_progress(100.0 * Y / maxY);
			Y = Y + stepover;
			X = maxX;
			if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
				line_to(input, X, Y, -maxZ + offset + get_height_tool(X, Y, radius + offset, ballnose));
			prevX = X;
			while (X > -overshoot) {
				double d;
				d = get_height_tool(X, Y, radius + offset, ballnose);
				if (fabs(d - last_Z) > 1 && roughing) {
					X = prevX - stepover / 1.5;
					d = get_height_tool(X, Y, radius + offset, ballnose);
				}

				if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
					line_to(input, X, Y, -maxZ + d + offset);
				prevX = X;
				X = X - stepover;
			}

			X = -overshoot;
			print_progress(100.0 * Y / maxY);
			Y = Y + stepover;
			if (Y < maxY && !outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
				line_to(input, X, Y, -maxZ + offset + get_height_tool(X, Y, radius + offset, ballnose));
		}
	}
	if (!even) {
		input = new(class inputshape);
		input->set_name("STL path");
		scene->shapes.push_back(input);
		first = true;
		X = -overshoot;
		while (X < maxX) {
			double prevY;
			Y = -overshoot;
			prevY = Y;
			while (Y < maxY) {
				double d;
				d = get_height_tool(X, Y, radius + offset, ballnose);
				if (fabs(d - last_Z) > 1 && roughing) {
					Y = prevY + stepover / 1.5;
					d = get_height_tool(X, Y, radius + offset, ballnose);
				}


				if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
					line_to(input, X, Y, -maxZ + d + offset);
				prevY = Y;
				Y = Y + stepover;
			}
			print_progress(100.0 * X / maxX);
			X = X + stepover;
			Y = maxY;
			if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam) &&  (X < maxX))
				line_to(input, X, Y, -maxZ + offset + get_height_tool(X, Y, radius + offset, ballnose));
			prevY = Y;
			while (Y > - overshoot) {
				double d;
				d = get_height_tool(X, Y, radius + offset, ballnose);
				if (fabs(d - last_Z) > 1 && roughing) {
					Y = prevY - stepover / 1.5;
					d = get_height_tool(X, Y, radius + offset, ballnose);
				}


				if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam))
					line_to(input, X, Y, -maxZ + d + offset);
				prevY = Y;
				Y = Y - stepover;
			}
			print_progress(100.0 * X / maxX);
			X = X + stepover;
			Y = -overshoot;

			if (!outside_area(X, Y, stl_image_X(), stl_image_Y(), diam) &&  (X < maxX))
					line_to(input, X, Y, -maxZ + offset + get_height_tool(X, Y, radius + offset, ballnose));

		}
	}

	printf("                                                          \r");
}

void process_stl_file(class scene *scene, const char *filename)
{
	bool omit_cutout = false;
	bool even = true;

	read_stl_file(filename);
	normalize_design_to_zero();

	if (scene->get_cutout_depth() < 0.01) {
		scene->set_cutout_depth(scene->get_depth());
		printf("Warning: No cutout depth set, using %5.2fmm for the model height\n", scene->get_cutout_depth());
		omit_cutout = true;
	}

	scale_design_Z(scene->get_cutout_depth());
	print_triangle_stats();


	for ( int i = scene->get_tool_count() - 1; i >= 0 ; i-- ) {
		activate_tool(scene->get_tool_nr(i));

		printf("Create toolpaths for tool %i \n", scene->get_tool_nr(i));

		/* only for the first roughing tool do we need to honor the max tool depth */
		if (i != 0) {
			tooldepth = 5000;
		} else {
			tooldepth = get_tool_maxdepth();
		}
		create_toolpath(scene, scene->get_tool_nr(i), i < (int)scene->get_tool_count() - 1, !omit_cutout, even);

		even = !even;
		if (i == (int)scene->get_tool_count() - 1 && scene->want_finishing_pass()) {
			create_toolpath(scene, scene->get_tool_nr(i), i < (int)scene->get_tool_count() - 1, !omit_cutout, even);

			even = !even;
		}
	}
	if (!omit_cutout) { 
		activate_tool(scene->get_tool_nr(0));
		create_cutout(scene, scene->get_tool_nr(0));
	}
}


