#include <iostream>
#include <map>
#include <set>

#include <stdlib.h>
#include <string.h>

extern "C" {
#include <sai.h>
#include "saimetadata.h"
#include "saimetadatautils.h"
}

// node name
#define NN(x) (sai_metadata_enum_sai_object_type_t.valuesshortnames[(x)])

std::set<sai_object_type_t> source;
std::set<sai_object_type_t> target;

bool show_switch_links = false;
bool show_read_only_links = false;

void process_object_type_attributes(
        _In_ const sai_attr_metadata_t** meta_attr_list,
        _In_ sai_object_type_t current_object_type)
{
    std::set<sai_object_type_t> otset;
    std::set<sai_object_type_t> rotset;

    for (int i = 0; meta_attr_list[i] != NULL; ++i)
    {
        const sai_attr_metadata_t* meta = meta_attr_list[i];

        if (meta->allowedobjecttypeslength == 0)
        {
            // skip attributes that don't contain object id's
            continue;
        }

        bool ro = SAI_HAS_FLAG_READ_ONLY(meta->flags);

        if (ro && !show_read_only_links)
        {
            // skip attributes that are read only
            continue;
        }

        std::string style;

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                // we can miss some objects if same object can be set
                // as list in one attribute and as single object in
                // another attribute
                style = "style=bold";

                break;

            default:
                break;
        }

        // this attribute supports objects

        if (meta->allowedobjecttypeslength > 1)
        {
            // point arrows to the same point origin when this is single attribute
            style += " samehead=" + std::string(meta->attridname);
        }

        for (uint32_t j = 0; j < meta->allowedobjecttypeslength; j++)
        {
            sai_object_type_t ot = meta->allowedobjecttypes[j];

            if (otset.find(ot) != otset.end())
            {
                // node was already defined
                continue;
            }

            const char* current = NN(current_object_type);
            const char* dep = NN(ot);

            if (ro)
            {
                if (rotset.find(ot) != rotset.end())
                {
                    continue;
                }

                rotset.insert(ot);
                std::cout << dep << " -> " << current << " [ " << style << " color=\"red\" ];\n";
                continue;
            }

            std::cout << dep << " -> " << current << " [ " << style << " color=\"0.650 0.700 0.700\"];\n";

            otset.insert(ot);

            source.insert(ot);
            target.insert(current_object_type);
        }
    }
}

void process_object_types()
{
    for (int i = 0; sai_metadata_attr_by_object_type[i] != NULL; ++i)
    {
        const sai_attr_metadata_t** meta = sai_metadata_attr_by_object_type[i];

        process_object_type_attributes(meta, (sai_object_type_t)i);
    }
}

void process_colors()
{
    for (int i = 0; sai_metadata_attr_by_object_type[i] != NULL; ++i)
    {
        sai_object_type_t ot = (sai_object_type_t)i;

        bool is_source = source.find(ot) != source.end();
        bool is_target = target.find(ot) != target.end();


        if (is_source && is_target)
        {
            // node is target and source, so it's in the middle
            std::cout << NN(ot) << " [color=\"0.650 0.500 1.000\"];\n";
        }
        else if (is_target)
        {
            // this node is a leaf
            std::cout << NN(ot) << " [color=\"0.355 0.563 1.000\", shape = rect];\n";

        }
        else if (is_source)
        {
            std::cout << NN(ot) << " [color=\"0.650 0.200 1.000\"];\n";
        }
        else
        {
            if (ot == SAI_OBJECT_TYPE_NULL || ot == SAI_OBJECT_TYPE_MAX)
            {
                continue;
            }
            std::cout << NN(ot) << " [color=plum, shape = rect];\n";
        }
    }

    for (size_t i = SAI_OBJECT_TYPE_NULL; i < SAI_OBJECT_TYPE_MAX; ++i)
    {
        const sai_object_type_info_t* oi =  sai_metadata_all_object_type_infos[i];

        if (oi == NULL)
        {
            continue;
        }

        if (!oi->isnonobjectid)
        {
            continue;
        }

        std::cout << NN(i) << " [color=plum, shape = rect];\n";
    }
}

#define PRINT_NN(x,y,c)\
    std::cout << NN(SAI_OBJECT_TYPE_ ## x) << " -> " << NN(SAI_OBJECT_TYPE_ ## y) << c;

void process_nonobjectid_connections()
{
    const char* c = " [color=\"0.650 0.700 0.700\", style = dashed, penwidth=2];\n";

    for (size_t i = SAI_OBJECT_TYPE_NULL; i < SAI_OBJECT_TYPE_MAX; ++i)
    {
        const sai_object_type_info_t* oi =  sai_metadata_all_object_type_infos[i];

        if (oi == NULL)
        {
            continue;
        }

        if (!oi->isnonobjectid)
        {
            continue;
        }

        for (size_t j = 0; j < oi->structmemberscount; ++j)
        {
            const sai_struct_member_info_t* sm = oi->structmembers[j];

            if (sm->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                for (size_t k = 0; k < sm->allowedobjecttypeslength; ++k)
                {
                    sai_object_type_t ot = sm->allowedobjecttypes[k];

                    if (ot == SAI_OBJECT_TYPE_SWITCH && !show_switch_links)
                    {
                        // skip switch dependency since switch
                        // is used everywhere and will pollute graph
                        continue;
                    }

                    std::cout << NN(ot) << " -> " << NN((sai_object_type_t)i) << c;
                }
            }
            else if (sm->isvlan)
            {
                std::cout << NN(SAI_OBJECT_TYPE_VLAN) << " -> " << NN((sai_object_type_t)i) << c;
            }
        }
    }

    PRINT_NN(PORT, SWITCH, "[dir=\"none\", color=\"red\", peripheries = 2, penwidth=2.0 , style  = dashed ];\n");
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        show_switch_links       |= strcmp(argv[i], "-s") == 0;
        show_read_only_links    |= strcmp(argv[i], "-r") == 0;
    }

    std::cout << "digraph \"SAI Object Dependency Graph\" {\n";
    std::cout << "size=\"30,12\"; ratio = fill;\n";
    std::cout << "node [style=filled];\n";

    process_object_types();

    process_nonobjectid_connections();

    process_colors();

    std::cout << NN(SAI_OBJECT_TYPE_SWITCH) << " [color=orange, shape = parallelogram, peripheries = 2];\n";
    std::cout << NN(SAI_OBJECT_TYPE_PORT) << " [color=gold, shape = diamond, peripheries=2];\n";

    std::cout << "}\n";
    return 0;
}
