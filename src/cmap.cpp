#include "cmap.h"

#include <sstream>
#include <fstream>
#include <limits>

std::vector<std::array<float, 3>> csv_read_cmap(std::istream& in)
{
    std::vector<std::array<float, 3>> cmap;
    std::string line;
    std::stringstream ss;
    while (std::getline(in, line)) {
        ss = std::stringstream(std::move(line));
        float r, g, b;
        ss >> r;
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ',');
        ss >> g;
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ',');
        ss >> b;

        cmap.push_back(std::array<float, 3>{r, g, b});
    }

    return cmap;
}

std::vector<std::array<float, 3>> csv_read_cmap(const std::string& path)
{
    std::ifstream f(path);

    return csv_read_cmap(f);
}
