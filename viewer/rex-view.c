/*
 * Copyright 2018 Robotic Eyes GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.*
 */
#include <stdbool.h>
#include <string.h>

#ifndef WIN32
#include <libgen.h>
#include <unistd.h>
#endif

#include "rex.h"
#include "list.h"

#include "gameengine.h"
#include "mesh.h"
#include "material.h"
#include "mesh_group.h"
#include "points.h"
#include "shader.h"

// This is the search path for the resources
char resource_search_path[256];

void usage (const char *exec)
{
    die ("usage: %s filename.rex\n", exec);
}

char *get_valid_resource_path()
{
#ifndef WIN32
    // get the path of the executable
    char binary_dir[256];
    readlink ("/proc/self/exe", binary_dir, 256);

    // check if we are in development mode
    sprintf (resource_search_path, "%s/../../viewer/shaders", dirname (binary_dir));
    if (dir_exists (resource_search_path))
        return resource_search_path;

    // take global path
    sprintf (resource_search_path, "%s/share/libopenrex", dirname (binary_dir));
    if (dir_exists (resource_search_path))
        return resource_search_path;
#else
    // TODO find a nice way to get the resource path
    sprintf (resource_search_path, ".%c", separator());
#endif
    return "";
}

void addmesh (struct rex_mesh *mesh, struct scene *s)
{
    if (!mesh)
        return;

    printf ("name                   %20s\n", (mesh->name) ? mesh->name : "");
    printf ("vertices               %20u\n", mesh->nr_vertices);
    printf ("triangles              %20u\n", mesh->nr_triangles);
    printf ("normals                %20s\n", (mesh->normals) ? "yes" : "no");
    printf ("texture coords         %20s\n", (mesh->tex_coords) ? "yes" : "no");
    printf ("vertex colors          %20s\n", (mesh->colors) ? "yes" : "no");

    struct mesh *m;
    m = malloc (sizeof (struct mesh));
    mesh_init (m);
    mesh_set_rex_mesh (m, mesh);
    scene_add_mesh (s, m);
    rex_mesh_free (mesh);
}

void addpoints (struct rex_pointlist *plist, struct scene *s)
{
    if (!plist)
        return;

    printf ("vertices               %20u\n", plist->nr_vertices);
    printf ("colors                 %20u\n", plist->nr_colors);

    points_set_rex_pointlist (&s->pointcloud, plist);
    scene_activate_pointcloud ();
}

void addlines (struct rex_lineset *ls, struct scene *s)
{
    if (!ls)
        return;

    struct polyline *p = malloc (sizeof (struct polyline));
    polyline_init (p);
    polyline_set_rex_lineset (p, ls);
    scene_add_polyline (s, p);
}

void addmaterial (uint64_t id, struct rex_material_standard *mat, struct list *l)
{
    if (!mat)
        return;

    struct material_standard *m = malloc (sizeof (struct material_standard));
    m->id = id;
    m->kd_red = mat->kd_red;
    m->kd_green = mat->kd_green;
    m->kd_blue = mat->kd_blue;
    list_insert(l, m);
}

/**
 * Load the complete REX file and generate geometry information
 * and store the information into the scene
 */
int loadrex (const char *file, struct scene *s)
{
    long sz;
    uint8_t *buf = read_file_binary (file, &sz);

    if (buf == NULL)
        die ("Cannot open REX file %s\n", file);

    struct rex_header header;
    uint8_t *ptr = rex_header_read (buf, &header);

    struct list *materials;
    materials = list_create();

    int geometry = 0;
    for (int i = 0; i < header.nr_datablocks; i++)
    {
        struct rex_block block;
        ptr = rex_block_read (ptr, &block);

        if (block.type == Mesh)
        {
            addmesh (block.data, s);
            rex_mesh_free (block.data);
            FREE (block.data);
            geometry++;
        }
        else if (block.type == PointList)
        {
            addpoints (block.data, s);
            FREE (block.data);
            geometry++;
        }
        else if (block.type == LineSet)
        {
            addlines (block.data, s);
            FREE (block.data);
            geometry++;
        }
        else if (block.type == MaterialStandard)
        {
            addmaterial(block.id, block.data, materials);
            FREE (block.data);
        }
    }

    // Assign materials to meshes
    struct node *cur = materials->head;
    while (cur)
    {
        struct material_standard *material = cur->data;

        // Assign material where materialID is matching
        struct node *meshIterator = s->meshes.meshes->head;
        while (meshIterator)
        {
            struct mesh *mesh = meshIterator->data;
            if (mesh->material_id == material->id)
            {
                mesh->diffuse_color[0] = material->kd_red;
                mesh->diffuse_color[1] = material->kd_green;
                mesh->diffuse_color[2] = material->kd_blue;
            }
            meshIterator = meshIterator->next;
        }

        cur = cur->next;
    }

    if (!geometry)
        die ("Nothing found to render, go home!");

    return 0;
}

int main (int argc, char **argv)
{
    printf ("═══════════════════════════════════════════\n");
    printf ("        %s %s (c) Robotic Eyes\n", rex_name, VERSION);
    printf ("═══════════════════════════════════════════\n\n");

    char *resource_path = get_valid_resource_path();
    printf ("Resource path: %s\n", resource_path);

    if (argc < 2)
        usage (argv[0]);

    gameengine_init();

    struct scene *s = scene_create (resource_path);

    if (loadrex (argv[1], s))
        die ("Unable to load REX file");

    gameengine_start (s);
    gameengine_cleanup();
    scene_destroy (s);

    return 0;
}
