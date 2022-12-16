#pragma once

#define MAX_TEXTURE 4096

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace layeredfs {

    struct Bitmap {
        string name;
        int width;
        int height;
        int packX;
        int packY;

        Bitmap(const string &name, int width, int height);
    };

    struct Packer {
        int width;
        int height;

        vector<Bitmap *> bitmaps;

        Packer(int max_size);

        void Pack(vector<Bitmap *> &bitmaps);
    };

    bool pack_textures(vector<Bitmap *> &textures, vector<Packer *> &packed_textures);
}
