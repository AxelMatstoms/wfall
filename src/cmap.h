#ifndef WFALL_CMAP_H
#define WFALL_CMAP_H

#include <iostream>
#include <vector>
#include <array>
#include <string>

std::vector<std::array<float, 3>> csv_read_cmap(std::istream& in);
std::vector<std::array<float, 3>> csv_read_cmap(const std::string& path);

#endif /* WFALL_CMAP_H */
