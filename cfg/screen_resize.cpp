#include "screen_resize.h"

#include "external/rapidjson/document.h"
#include "external/rapidjson/writer.h"
#include "util/utils.h"
#include "util/fileutils.h"

using namespace rapidjson;

namespace cfg {

    // globals
    std::unique_ptr<cfg::ScreenResize> SCREENRESIZE;

    ScreenResize::ScreenResize() {
        this->config_path = std::string(getenv("APPDATA")) + "\\spicetools_screen_resize.json";
        if (fileutils::file_exists(this->config_path)) {
            this->config_load();
        }
    }

    ScreenResize::~ScreenResize() {
    }
    
    void ScreenResize::config_load() {
        log_info("ScreenResize", "loading config");

        std::string config = fileutils::text_read(this->config_path);
        if (!config.empty()) {

            // parse document
            Document doc;
            doc.Parse(config.c_str());

            // check parse error
            auto error = doc.GetParseError();
            if (error) {
                log_warning("ScreenResize", "config parse error: {}", error);
            }

            // verify root is a dict
            if (doc.IsObject()) {
                bool valid = true;
                auto offset_x = doc.FindMember("offset_x");
                if (offset_x == doc.MemberEnd() || !offset_x->value.IsInt()) {
                    log_warning("ScreenResize", "offset_x not found");
                    valid = false;
                }
                auto offset_y = doc.FindMember("offset_y");
                if (offset_y == doc.MemberEnd() || !offset_y->value.IsInt()) {
                    log_warning("ScreenResize", "offset_y not found");
                    valid = false;
                }
                auto scale_x = doc.FindMember("scale_x");
                if (scale_x == doc.MemberEnd() || !scale_x->value.IsDouble()) {
                    log_warning("ScreenResize", "scale_x not found");
                    valid = false;
                }
                auto scale_y = doc.FindMember("scale_y");
                if (scale_y == doc.MemberEnd() || !scale_y->value.IsDouble()) {
                    log_warning("ScreenResize", "scale_y not found");
                    valid = false;
                }
                auto enable_screen_resize = doc.FindMember("enable_screen_resize");
                if (enable_screen_resize == doc.MemberEnd() || !enable_screen_resize->value.IsBool()) {
                    log_warning("ScreenResize", "enable_screen_resize not found");
                    valid = false;
                }
                auto enable_linear_filter = doc.FindMember("enable_linear_filter");
                if (enable_linear_filter == doc.MemberEnd() || !enable_linear_filter->value.IsBool()) {
                    log_warning("ScreenResize", "enable_linear_filter not found");
                    valid = false;
                }
                auto keep_aspect_ratio = doc.FindMember("keep_aspect_ratio");
                if (keep_aspect_ratio == doc.MemberEnd() || !keep_aspect_ratio->value.IsBool()) {
                    log_warning("ScreenResize", "keep_aspect_ratio not found");
                    valid = false;
                }
                if (valid) {
                    this->offset_x = offset_x->value.GetInt();
                    this->offset_y = offset_y->value.GetInt();
                    this->scale_x = (float)scale_x->value.GetDouble();
                    this->scale_y = (float)scale_y->value.GetDouble();
                    this->enable_screen_resize = enable_screen_resize->value.GetBool();
                    this->enable_linear_filter = enable_linear_filter->value.GetBool();
                    this->keep_aspect_ratio = keep_aspect_ratio->value.GetBool();
                }
            } else {
                log_warning("ScreenResize", "config not found");
            }
        }
    }

    void ScreenResize::config_save() {
        log_info("ScreenResize", "saving config");

        // create document
        Document doc;
        doc.SetObject();
        auto &alloc = doc.GetAllocator();
        
        doc.AddMember("offset_x", this->offset_x, alloc);
        doc.AddMember("offset_y", this->offset_y, alloc);
        doc.AddMember("scale_x", this->scale_x, alloc);
        doc.AddMember("scale_y", this->scale_y, alloc);
        doc.AddMember("enable_screen_resize", this->enable_screen_resize, alloc);
        doc.AddMember("enable_linear_filter", this->enable_linear_filter, alloc);
        doc.AddMember("keep_aspect_ratio", this->keep_aspect_ratio, alloc);
        
        // build JSON
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);

        // save to file
        if (fileutils::text_write(this->config_path, buffer.GetString())) {
            // this->config_dirty = false;
        } else {
            log_warning("ScreenResize", "unable to save config file to {}", this->config_path);
        }
    }
}
