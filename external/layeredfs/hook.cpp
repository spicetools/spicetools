#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "hook.h"

// all good code mixes C and C++, right?
using std::string;

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <fstream>

#include "external/hash-library/md5.h"
#include "3rd_party/lodepng.h"
#include "3rd_party/stb_dxt.h"
#include "3rd_party/GuillotineBinPack.h"
#include "3rd_party/rapidxml_print.hpp"

#include "util/detour.h"
#include "config.h"
#include "utils.h"
#include "texture_packer.h"
#include "modpath_handler.h"

// let me use the std:: version, dammit
#undef max
#undef min

#define VER_STRING "1.9-SPICE"

#ifdef _DEBUG
#define DBG_VER_STRING "_DEBUG"
#else
#define DBG_VER_STRING
#endif

#define VERSION VER_STRING DBG_VER_STRING

// debugging
//#define ALWAYS_CACHE

namespace layeredfs {

    time_t dll_time{};
    bool initialized;

    enum img_format {
        ARGB8888REV,
        DXT5,
        UNSUPPORTED_FORMAT,
    };

    enum compress_type {
        NONE,
        AVSLZ,
        UNSUPPORTED_COMPRESS,
    };

    typedef struct image {
        string name;
        string name_md5;
        img_format format;
        compress_type compression;
        string ifs_mod_path;
        int width;
        int height;

        const string cache_folder() { return CACHE_FOLDER "/" + ifs_mod_path; }

        const string cache_file() { return cache_folder() + "/" + name_md5; };
    } image_t;

// ifs_textures["data/graphics/ver04/logo.ifs/tex/4f754d4f424f092637a49a5527ece9bb"] will be "konami"
    std::unordered_map<string, image_t> ifs_textures;

    typedef std::unordered_set<string> string_set;

    char *avs_file_to_string(avs::core::avs_file_t f, rapidxml::xml_document<> &allocator) {
        avs::core::avs_stat stat{};
        avs::core::avs_fs_fstat(f, &stat);
        char *ret = allocator.allocate_string(nullptr, stat.filesize + 1);
        avs::core::avs_fs_read(f, (uint8_t *) ret, stat.filesize);
        ret[stat.filesize] = '\0';
        return ret;
    }

    bool is_binary_prop(avs::core::avs_file_t f) {
        avs::core::avs_fs_lseek(f, 0, SEEK_SET);
        unsigned char head;
        auto read = avs::core::avs_fs_read(f, &head, 1);
        bool ret = (read == 1) && head == 0xA0;
        avs::core::avs_fs_lseek(f, 0, SEEK_SET);
        //logf("detected binary: %s (read %d byte %02x)", ret ? "true": "false", read, head&0xff);
        return ret;
    }

    avs::core::property_ptr prop_from_file_handle(avs::core::avs_file_t f) {
        void *prop_buffer = nullptr;
        avs::core::property_ptr prop = nullptr;
        int ret = 0;

        int flags =
                avs::core::PROP_READ |
                avs::core::PROP_WRITE |
                avs::core::PROP_CREATE |
                avs::core::PROP_APPEND |
                avs::core::PROP_BIN_PLAIN_NODE_NAMES;
        auto memsize = avs::core::property_read_query_memsize_long(
                (avs::core::avs_reader_t) avs::core::avs_fs_read,
                f, nullptr, nullptr, nullptr);
        if (memsize < 0) {
            // normal prop
            flags &= ~avs::core::PROP_BIN_PLAIN_NODE_NAMES;

            avs::core::avs_fs_lseek(f, 0, SEEK_SET);
            memsize = avs::core::property_read_query_memsize(
                    (avs::core::avs_reader_t) avs::core::avs_fs_read,
                    f, nullptr, nullptr);
            if (memsize < 0) {
                logf("Couldn't get memsize %08X (%s)", memsize, avs::core::error_str(memsize).c_str());
                goto FAIL;
            }
        }

        prop_buffer = malloc(memsize);
        prop = avs::core::property_create(flags, prop_buffer, memsize);
        if ((int32_t) (intptr_t) prop < 0) {
            logf("Couldn't create prop (%s)", avs::core::error_str((int32_t) (intptr_t) prop).c_str());
            goto FAIL;
        }

        avs::core::avs_fs_lseek(f, 0, SEEK_SET);
        ret = avs::core::property_insert_read(prop, 0, (avs::core::avs_reader_t) avs::core::avs_fs_read, f);
        avs::core::avs_fs_close(f);

        if (ret < 0) {
            logf("Couldn't read prop (%s)", avs::core::error_str(ret).c_str());
            goto FAIL;
        }

        return prop;

FAIL:
        avs::core::avs_fs_close(f);

        if (prop) {
            avs::core::property_destroy(prop);
        }
        if (prop_buffer) {
            free(prop_buffer);
        }

        return nullptr;
    }

    char *prop_to_xml_string(avs::core::property_ptr prop, rapidxml::xml_document<> &allocator) {
        avs::core::node_stat dummy{};

        auto prop_size = avs::core::property_node_query_stat(prop, nullptr, &dummy);
        char *xml = allocator.allocate_string(nullptr, prop_size);

        auto written = avs::core::property_mem_write(prop, (uint8_t *) xml, prop_size);
        if (written > 0) {
            xml[written] = '\0';
        } else {
            xml[0] = '\0';
            logf("property_mem_write failed (%s)", avs::core::error_str(written).c_str());
        }

        return xml;
    }

    void rapidxml_dump_to_file(const string &out, const rapidxml::xml_document<> &xml) {
        std::ofstream out_file;
        out_file.open(out.c_str());

        // this is 3x faster than writing directly to the output file
        std::string s;
        print(std::back_inserter(s), xml, rapidxml::print_no_indenting);
        out_file << s;

        out_file.close();
    }

    bool rapidxml_from_avs_filepath(
            string const &path,
            rapidxml::xml_document<> &doc,
            rapidxml::xml_document<> &doc_to_allocate_with
    ) {
        avs::core::avs_file_t f = avs::core::avs_fs_open(path.c_str(), 1, 420);
        if ((int32_t) f < 0) {
            logf("Couldn't open prop");
            return false;
        }

        // if it's not binary, don't even bother parsing with avs
        char *xml = nullptr;
        if (is_binary_prop(f)) {
            auto prop = prop_from_file_handle(f);
            if (!prop)
                return false;

            xml = prop_to_xml_string(prop, doc_to_allocate_with);

            // clean up
            auto buffer = avs::core::property_desc_to_buffer(prop);
            avs::core::property_destroy(prop);
            if (buffer) {
                free(buffer);
            }
        } else {
            xml = avs_file_to_string(f, doc_to_allocate_with);
        }
        avs::core::avs_fs_close(f);

        try {
            // parse_declaration_node: to get the header <?xml version="1.0" encoding="shift-jis"?>
            doc.parse<rapidxml::parse_declaration_node | rapidxml::parse_no_utf8>(xml);
        } catch (const rapidxml::parse_error &e) {
            logf("Couldn't parse xml (%s byte %d)", e.what(), (int) (e.where<char>() - xml));
            return false;
        }

        return true;
    }

    void list_pngs_onefolder(string_set &names, string const &folder) {
        auto search_path = folder + "/*.png";
        const auto extension_len = strlen(".png");
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // read all (real) files in current folder
                // , delete '!' read other 2 default folder . and ..
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    fd.cFileName[strlen(fd.cFileName) - extension_len] = '\0';
                    names.insert(fd.cFileName);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    string_set list_pngs(string const &folder) {
        string_set ret;

        for (auto &mod : available_mods()) {
            auto path = mod + "/" + folder;
            list_pngs_onefolder(ret, path);
            list_pngs_onefolder(ret, path + "/tex");
        }

        return ret;
    }

#define FMT_4U16(arr) "%u %u %u %u", (arr)[0], (arr)[1], (arr)[2], (arr)[3]
#define FMT_2U16(arr) "%u %u", (arr)[0], (arr)[1]

    rapidxml::xml_node<> *allocate_node_and_attrib(
            rapidxml::xml_document<> *doc,
            const char *node_name,
            const char *node_value,
            const char *attr_name,
            const char *attr_value) {
        auto node = doc->allocate_node(rapidxml::node_element, node_name, node_value);
        node->append_attribute(doc->allocate_attribute(attr_name, attr_value));
        return node;
    }

    bool add_images_to_list(string_set &extra_pngs, rapidxml::xml_node<> *texturelist_node,
                            string const &ifs_path, string const &ifs_mod_path, compress_type compress) {
        auto start = time();
        vector<Bitmap *> textures;

        for (auto it = extra_pngs.begin(); it != extra_pngs.end(); ++it) {
            logf_verbose("New image: %s", it->c_str());

            string png_tex = *it + ".png";
            auto png_loc = find_first_modfile(ifs_mod_path + "/" + png_tex);
            if (!png_loc)
                png_loc = find_first_modfile(ifs_mod_path + "/tex/" + png_tex);
            if (!png_loc)
                continue;

            FILE *f = fopen(png_loc->c_str(), "rb");
            if (!f) // shouldn't happen but check anyway
                continue;

            unsigned char header[33];
            // this may read less bytes than expected but lodepng will die later anyway
            fread(header, 1, 33, f);
            fclose(f);

            unsigned width, height;
            LodePNGState state = {};
            if (lodepng_inspect(&width, &height, &state, header, 33)) {
                logf("couldn't inspect png");
                continue;
            }

            textures.push_back(new Bitmap(*it, width, height));
        }

        auto pack_start = time();
        vector<Packer *> packed_textures;
        logf("Packing textures for %d textures", textures.size());
        if (!pack_textures(textures, packed_textures)) {
            logf("Couldn't pack textures :(");
            return false;
        }
        logf("Textures packed in %d ms into %d objects", time() - pack_start, packed_textures.size());

        // because the property API, being
        // a) written by Konami
        // b) not documented since lol DLLs
        // is absolutely garbage to work with for merging XMLs, we use rapidxml instead
        // thus I hope my sanity is restored.

        auto document = texturelist_node->document();
        for (unsigned int i = 0; i < packed_textures.size(); i++) {
            Packer *canvas = packed_textures[i];
            char tex_name[8];
            snprintf(tex_name, 8, "ctex%03d", i);
            auto canvas_node = document->allocate_node(rapidxml::node_element, "texture");
            texturelist_node->append_node(canvas_node);
            canvas_node->append_attribute(document->allocate_attribute("format", "argb8888rev"));
            canvas_node->append_attribute(document->allocate_attribute("mag_filter", "nearest"));
            canvas_node->append_attribute(document->allocate_attribute("min_filter", "nearest"));
            canvas_node->append_attribute(document->allocate_attribute("name", document->allocate_string(tex_name)));
            canvas_node->append_attribute(document->allocate_attribute("wrap_s", "clamp"));
            canvas_node->append_attribute(document->allocate_attribute("wrap_t", "clamp"));

            char tmp[64];

            uint16_t size[2] = {(uint16_t) canvas->width, (uint16_t) canvas->height};

            snprintf(tmp, sizeof(tmp), FMT_2U16(size));
            canvas_node->append_node(
                    allocate_node_and_attrib(document, "size", document->allocate_string(tmp), "__type", "2u16"));

            for (unsigned int j = 0; j < canvas->bitmaps.size(); j++) {
                Bitmap *texture = canvas->bitmaps[j];
                auto tex_node = document->allocate_node(rapidxml::node_element, "image");
                canvas_node->append_node(tex_node);
                tex_node->append_attribute(
                        document->allocate_attribute("name", document->allocate_string(texture->name.c_str())));

                uint16_t coords[4];
                coords[0] = texture->packX * 2;
                coords[1] = (texture->packX + texture->width) * 2;
                coords[2] = texture->packY * 2;
                coords[3] = (texture->packY + texture->height) * 2;
                snprintf(tmp, sizeof(tmp), FMT_4U16(coords));
                tex_node->append_node(
                        allocate_node_and_attrib(document, "imgrect", document->allocate_string(tmp), "__type",
                                                 "4u16"));
                coords[0] += 2;
                coords[1] -= 2;
                coords[2] += 2;
                coords[3] -= 2;
                snprintf(tmp, sizeof(tmp), FMT_4U16(coords));
                tex_node->append_node(
                        allocate_node_and_attrib(document, "uvrect", document->allocate_string(tmp), "__type", "4u16"));

                // generate md5
                MD5 md5_tex_name;
                md5_tex_name.add(texture->name.c_str(), texture->name.length());

                // build image info
                image_t image_info;
                image_info.name = texture->name;
                image_info.name_md5 = md5_tex_name.getHash();
                image_info.format = ARGB8888REV;
                image_info.compression = compress;
                image_info.ifs_mod_path = ifs_mod_path;
                image_info.width = texture->width;
                image_info.height = texture->height;

                auto md5_path = ifs_path + "/tex/" + image_info.name_md5;
                ifs_textures[md5_path] = image_info;
            }
        }

        logf("Texture extend total time %d ms", time() - start);
        return true;
    }

    void parse_texturelist(string const &path, string const &norm_path, optional<string> &mod_path) {
        bool prop_was_rewritten = false;

        // get a reasonable base path
        auto ifs_path = norm_path;
        ifs_path.resize(ifs_path.size() - strlen("/tex/texturelist.xml"));
        logf_verbose("Reading ifs %s", ifs_path.c_str());
        auto ifs_mod_path = ifs_path;
        string_replace(ifs_mod_path, ".ifs", "_ifs");

        // TODO ifs_path gets invalid later this function for unknown reasons
        auto ifs_path2 = ifs_path;

        if (!find_first_modfolder(ifs_mod_path)) {
            logf_verbose("mod folder doesn't exist, skipping");
            return;
        }

        // open the correct file
        auto path_to_open = mod_path ? *mod_path : path;
        rapidxml::xml_document<> texturelist;
        auto success = rapidxml_from_avs_filepath(path_to_open, texturelist, texturelist);
        if (!success)
            return;

        auto texturelist_node = texturelist.first_node("texturelist");

        if (!texturelist_node) {
            logf("texlist has no texturelist node");
            return;
        }

        auto extra_pngs = list_pngs(ifs_mod_path);

        auto compress = NONE;
        rapidxml::xml_attribute<> *compress_node;
        if ((compress_node = texturelist_node->first_attribute("compress"))) {
            if (!_stricmp(compress_node->value(), "avslz")) {
                compress = AVSLZ;
            } else {
                compress = UNSUPPORTED_COMPRESS;
            }
        }

        for (auto texture = texturelist_node->first_node("texture");
             texture;
             texture = texture->next_sibling("texture")) {

            auto format = texture->first_attribute("format");
            if (!format) {
                logf("Texture missing format %s", path_to_open.c_str());
                continue;
            }

            //<size __type="2u16">128 128</size>
            auto size = texture->first_node("size");
            if (!size) {
                logf("Texture missing size %s", path_to_open.c_str());
                continue;
            }

            auto format_type = UNSUPPORTED_FORMAT;
            if (!_stricmp(format->value(), "argb8888rev")) {
                format_type = ARGB8888REV;
            } else if (!_stricmp(format->value(), "dxt5")) {
                format_type = DXT5;
            }

            for (auto image = texture->first_node("image");
                 image;
                 image = image->next_sibling("image")) {
                auto name = image->first_attribute("name");
                if (!name) {
                    logf("Texture missing name %s", path_to_open.c_str());
                    continue;
                }

                uint16_t dimensions[4];
                auto imgrect = image->first_node("imgrect");
                auto uvrect = image->first_node("uvrect");
                if (!imgrect || !uvrect) {
                    logf("Texture missing dimensions %s", path_to_open.c_str());
                    continue;
                }

                // it's a 4u16
                sscanf(imgrect->value(), "%hu %hu %hu %hu",
                        &dimensions[0],
                        &dimensions[1],
                        &dimensions[2],
                        &dimensions[3]);

                // generate md5
                MD5 md5_name;
                md5_name.add(name->value(), strlen(name->value()));

                //logf("Image '%s' compress %d format %d", tmp, compress, format_type);
                image_t image_info;
                image_info.name = name->value();
                image_info.name_md5 = md5_name.getHash();
                image_info.format = format_type;
                image_info.compression = compress;
                image_info.ifs_mod_path = ifs_mod_path;
                image_info.width = (dimensions[1] - dimensions[0]) / 2;
                image_info.height = (dimensions[3] - dimensions[2]) / 2;

                auto md5_path = ifs_path2 + "/tex/" + image_info.name_md5;
                ifs_textures[md5_path] = image_info;

                // TODO: why does this make it not crash
                if (config.verbose_logs) {
                    log_info("md5", "{}", md5_path);
                }

                extra_pngs.erase(image_info.name);
            }
        }

        logf_verbose("%d added PNGs for %s", extra_pngs.size(), ifs_path2.c_str());
        if (!extra_pngs.empty()) {
            if (add_images_to_list(extra_pngs, texturelist_node, ifs_path2, ifs_mod_path, compress))
                prop_was_rewritten = true;
        }

        if (prop_was_rewritten) {
            string outfolder = CACHE_FOLDER "/" + ifs_mod_path;
            if (!mkdir_p(outfolder)) {
                logf("Couldn't create cache folder");
            }
            string outfile = outfolder + "/texturelist.xml";
            rapidxml_dump_to_file(outfile, texturelist);
            mod_path = outfile;
        }
    }

    bool cache_texture(string const &png_path, image_t &tex) {
        string cache_path = tex.cache_folder();
        if (!mkdir_p(cache_path)) {
            logf("Couldn't create texture cache folder");
            return false;
        }

        string cache_file = tex.cache_file();
        auto cache_time = file_time(cache_file.c_str());
        auto png_time = file_time(png_path.c_str());

        // the cache is fresh, don't do the same work twice
#ifndef ALWAYS_CACHE
        if (cache_time > 0 && cache_time >= dll_time && cache_time >= png_time) {
            return true;
        }
#endif

        // make the cache
        FILE *cache;

        unsigned error;
        unsigned char *image;
        unsigned width, height; // TODO use these to check against xml

        error = lodepng_decode32_file(&image, &width, &height, png_path.c_str());
        if (error) {
            logf("can't load png %u: %s\n", error, lodepng_error_text(error));
            return false;
        }

        if ((int) width != tex.width || (int) height != tex.height) {
            logf("Loaded png (%dx%d) doesn't match texturelist.xml (%dx%d), ignoring", width, height, tex.width,
                 tex.height);
            return false;
        }

        size_t image_size = 4 * width * height;

        switch (tex.format) {
            case ARGB8888REV:
                for (size_t i = 0; i < image_size; i += 4) {
                    // swap r and b
                    auto tmp = image[i];
                    image[i] = image[i + 2];
                    image[i + 2] = tmp;
                }
                break;
            case DXT5: {
                size_t dxt5_size = image_size / 4;
                auto *dxt5_image = (unsigned char *) malloc(dxt5_size);
                rygCompress(dxt5_image, image, width, height, 1);
                free(image);
                image = dxt5_image;
                image_size = dxt5_size;

                // the data has swapped endianness for every WORD
                for (size_t i = 0; i < image_size; i += 2) {
                    auto tmp = image[i];
                    image[i] = image[i + 1];
                    image[i + 1] = tmp;
                }

                break;
            }
            default:
                break;
        }
        auto uncompressed_size = image_size;

        if (tex.compression == AVSLZ) {
            size_t compressed_size;
            auto compressed = lz_compress(image, image_size, &compressed_size);
            free(image);
            if (compressed == nullptr) {
                logf("Couldn't compress");
                return false;
            }
            image = compressed;
            image_size = compressed_size;
        }

        cache = std::fopen(cache_file.c_str(), "wb");
        if (!cache) {
            logf("can't open cache for writing");
            return false;
        }
        if (tex.compression == AVSLZ) {
            uint32_t uncomp_sz = _byteswap_ulong((uint32_t) uncompressed_size);
            uint32_t comp_sz = _byteswap_ulong((uint32_t) image_size);
            fwrite(&uncomp_sz, 4, 1, cache);
            fwrite(&comp_sz, 4, 1, cache);
        }
        fwrite(image, 1, image_size, cache);
        fclose(cache);
        free(image);
        return true;
    }

    void handle_texture(string const &norm_path, optional<string> &mod_path) {
        logf_verbose("handle_texture");
        auto tex_search = ifs_textures.find(norm_path);
        if (tex_search == ifs_textures.end()) {
            return;
        }

        logf_verbose("Mapped file %s is found!", norm_path.c_str());
        auto tex = tex_search->second;

        // remove the /tex/, it's nicer to navigate
        auto png_path = find_first_modfile(tex.ifs_mod_path + "/" + tex.name + ".png");
        if (!png_path) {
            // but maybe they used it anyway
            png_path = find_first_modfile(tex.ifs_mod_path + "/tex/" + tex.name + ".png");
            if (!png_path)
                return;
        }

        if (tex.compression == UNSUPPORTED_COMPRESS) {
            logf("Unsupported compression for %s", png_path->c_str());
            return;
        }
        if (tex.format == UNSUPPORTED_FORMAT) {
            logf("Unsupported texture format for %s", png_path->c_str());
            return;
        }

        logf_verbose("Mapped file %s found!", png_path->c_str());
        if (cache_texture(*png_path, tex)) {
            mod_path = tex.cache_file();
        }
    }

    void hash_filenames(vector<string> &filenames, uint8_t hash[16]) {
        MD5 digest;
        for (auto &path : filenames) {
            digest.add(path.c_str(), path.length());
        }
        digest.getHash(hash);
    }

    void merge_xmls(string const &path, string const &norm_path, optional<string> &mod_path) {
        auto start = time();
        string out;
        string out_folder;
        rapidxml::xml_document<> merged_xml;

        auto merge_path = norm_path;
        string_replace(merge_path, ".xml", ".merged.xml");
        auto to_merge = find_all_modfile(merge_path);
        // nothing to do...
        if (to_merge.empty()) {
            return;
        }

        auto starting = mod_path ? *mod_path : path;
        out = CACHE_FOLDER "/" + norm_path;
        auto out_hashed = out + ".hashed";

        uint8_t hash[16];
        hash_filenames(to_merge, hash);

        uint8_t cache_hash[16] = {0};
        FILE *cache_hashfile{};
        cache_hashfile = std::fopen(out_hashed.c_str(), "rb");
        if (cache_hashfile) {
            fread(cache_hash, sizeof(uint8_t), std::size(cache_hash), cache_hashfile);
            fclose(cache_hashfile);
        }

        auto time_out = file_time(out.c_str());
        // don't forget to take the input into account
        time_t newest = file_time(starting.c_str());
        for (auto &path : to_merge)
            newest = std::max(newest, file_time(path.c_str()));
        // no need to merge - timestamps all up to date, dll not newer, files haven't been deleted
        if (time_out >= newest && time_out >= dll_time && memcmp(hash, cache_hash, sizeof(hash)) == 0) {
            mod_path = out;
            return;
        }

        auto first_result = rapidxml_from_avs_filepath(starting, merged_xml, merged_xml);
        if (!first_result) {
            logf("Couldn't merge (can't load first xml %s)", starting.c_str());
            return;
        }

        logf("Merging into %s", starting.c_str());
        for (auto &path : to_merge) {
            logf("  %s", path.c_str());
            rapidxml::xml_document<> rapid_to_merge;
            auto merge_load_result = rapidxml_from_avs_filepath(path, rapid_to_merge, merged_xml);
            if (!merge_load_result) {
                logf("Couldn't merge (can't load xml) %s", path.c_str());
                return;
            }

            // toplevel nodes include doc declaration and mdb node
            // getting the last node grabs the mdb node
            // document -> mdb entry -> music entry
            for (rapidxml::xml_node<> *node = rapid_to_merge.last_node()->first_node(); node; node = node->next_sibling()) {
                merged_xml.last_node()->append_node(merged_xml.clone_node(node));
            }
        }

        auto folder_terminator = out.rfind("/");
        out_folder = out.substr(0, folder_terminator);
        if (!mkdir_p(out_folder)) {
            logf("Couldn't create merged cache folder");
        }

        rapidxml_dump_to_file(out, merged_xml);
        cache_hashfile = std::fopen(out_hashed.c_str(), "wb");
        if (cache_hashfile) {
            fwrite(hash, 1, sizeof(hash), cache_hashfile);
            fclose(cache_hashfile);
        }
        mod_path = out;

        logf("Merge took %d ms", time() - start);
    }

    int hook_avs_fs_lstat(const char *name, struct avs::core::avs_stat *st) {
        if (name == nullptr)
            return avs::core::avs_fs_lstat(name, st);

        logf_verbose("statting %s", name);
        string path = name;

        // can it be modded?
        auto norm_path = normalise_path(path);
        if (!norm_path)
            return avs::core::avs_fs_lstat(name, st);

        auto mod_path = find_first_modfile(*norm_path);

        if (mod_path) {
            logf_verbose("Overwriting lstat");
            return avs::core::avs_fs_lstat(mod_path->c_str(), st);
        }
        return avs::core::avs_fs_lstat(name, st);
    }

    avs::core::avs_file_t hook_avs_fs_open(const char *name, uint16_t mode, int flags) {
        if (name == nullptr)
            return avs::core::avs_fs_open(name, mode, flags);
        logf_verbose("opening %s mode %d flags %d", name, mode, flags);

        // only touch reads
        if (mode != 1) {
            return avs::core::avs_fs_open(name, mode, flags);
        }
        string path = name;

        // can it be modded ie is it under /data ?
        auto _norm_path = normalise_path(path);
        if (!_norm_path)
            return avs::core::avs_fs_open(name, mode, flags);
        // unpack success
        auto norm_path = *_norm_path;

        auto mod_path = find_first_modfile(norm_path);
        if (!mod_path) {
            // mod ifs paths use _ifs
            string_replace(norm_path, ".ifs", "_ifs");
            mod_path = find_first_modfile(norm_path);
        }

        if (string_ends_with(path.c_str(), ".xml")) {
            merge_xmls(path, norm_path, mod_path);
        }

        if (string_ends_with(path.c_str(), "texturelist.xml")) {
            parse_texturelist(path, norm_path, mod_path);
        } else {
            handle_texture(norm_path, mod_path);
        }

        if (mod_path) {
            logf("Using %s", mod_path->c_str());
        }

        auto to_open = mod_path ? *mod_path : path;
        auto ret = avs::core::avs_fs_open(to_open.c_str(), mode, flags);
        logf_verbose("returned %d", ret);
        return ret;
    }

    int init() {
        logf("IFS layeredFS v"
                     VERSION
                     " init...");

        // init DLL time
        char dll_filename[MAX_PATH];
        if (GetModuleFileNameA(GetModuleHandle(nullptr), dll_filename, sizeof(dll_filename))) {
            dll_time = file_time(dll_filename);
        }

        // init functions
        load_config();
        cache_mods();

        // check if required functions are available
        if (avs::core::avs_fs_open == nullptr || avs::core::avs_fs_lstat == nullptr
            || avs::core::property_node_read == nullptr) {
            logf("optional avs imports not available :(");
            return 1;
        }

        logf("Hooking ifs operations");
        initialized = true;

        logf("Detected mod folders:");
        for (auto &p : available_mods()) {
            logf("%s", p.c_str());
        }

        return 0;
    }

}
