#include "texture_packer.h"

#include <algorithm>

#include "3rd_party/GuillotineBinPack.h"

using namespace rbp;

namespace layeredfs {

    Bitmap::Bitmap(const string &name, int width, int height)
            : name(name), width(width), height(height) {

    }

    bool pack_textures(vector<Bitmap *> &textures, vector<Packer *> &packed_textures) {
        std::sort(textures.begin(), textures.end(), [](const Bitmap *a, const Bitmap *b) {
            return (a->width * a->height) < (b->width * b->height);
        });

        // pack the bitmaps
        while (!textures.empty()) {
            auto packer = new Packer(MAX_TEXTURE);
            packer->Pack(textures);
            packed_textures.push_back(packer);

            // failed
            if (packer->bitmaps.empty())
                return false;
        }

        return true;
    }

    Packer::Packer(int max_size)
            : width(max_size), height(max_size) {

    }

    void Packer::Pack(vector<Bitmap *> &bitmaps) {
        GuillotineBinPack packer(width, height);

        int ww = 0;
        int hh = 0;
        while (!bitmaps.empty()) {
            auto bitmap = bitmaps.back();

            Rect rect = packer.Insert(bitmap->width, bitmap->height, false,
                                      GuillotineBinPack::FreeRectChoiceHeuristic::RectBestAreaFit,
                                      GuillotineBinPack::GuillotineSplitHeuristic::SplitLongerAxis);

            if (rect.width == 0 || rect.height == 0)
                break;

            bitmap->packX = rect.x;
            bitmap->packY = rect.y;
            this->bitmaps.push_back(bitmap);
            bitmaps.pop_back();

            ww = std::max(rect.x + rect.width, ww);
            hh = std::max(rect.y + rect.height, hh);
        }

        while (width / 2 >= ww)
            width /= 2;
        while (height / 2 >= hh)
            height /= 2;
    }
}
