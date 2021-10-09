#ifndef WFALL_MATRIX_H
#define WFALL_MATRIX_H

#include <array>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <stdexcept>

template <std::size_t R, std::size_t C>
concept square = requires {R == C;};

template <std::size_t R, std::size_t C>
struct Column {
    const std::array<float, R * C>& _data;
    std::size_t c;

    Column(const std::array<float, R * C>& data, std::size_t c) : _data(data), c(c) {}

    const float& operator[](std::size_t r) const
    {
        return _data[r * C + c];
    }
};

template <std::size_t R, std::size_t C>
struct Row {
    const std::array<float, R * C>& _data;
    std::size_t r;

    Row(const std::array<float, R * C>& data, std::size_t r) : _data(data), r(r) {}

    const float& operator[](std::size_t c) const
    {
        return _data[r * C + c];
    }
};

template <std::size_t R, std::size_t C>
struct Matrix {
    std::array<float, R * C> _data;

    Matrix()
    {
        for (std::size_t i = 0; i < R * C; i++) {
            _data[i] = 0.0f;
        }
    }

    Matrix(const std::array<float, R * C> data) : _data(data) {}

    Matrix(const std::initializer_list<std::initializer_list<float>> init)
    {
        if (init.size() != R) {
            throw std::invalid_argument("Initializer list contains wrong number of rows");
        }
        for (auto [r, itr] = std::make_pair(0u, init.begin()); itr != init.end(); ++r, ++itr) {
            if (itr->size() != C) {
                throw std::invalid_argument("A row in the initializer list contains the wrong number of elements.");
            }
            for (auto [c, itr2] = std::make_pair(0u, itr->begin()); itr2 != itr->end(); ++c, ++itr2) {
                (*this)(r, c) = *itr2;
            }
        }
    }

    float& operator()(std::size_t r, std::size_t c)
    {
        return _data[C * r + c];
    }

    const float& operator()(std::size_t r, std::size_t c) const
    {
        return _data[C * r + c];
    }

    Row<R, C> row(std::size_t r) const
    {
        return Row<R, C>(_data, r);
    }

    Column<R, C> column(std::size_t c) const
    {
        return Column<R, C>(_data, c);
    }

    const float* data() const
    {
        return _data.data();
    }

    Matrix& operator+=(const Matrix& other)
    {
        for (std::size_t i = 0; i < R * C; i++) {
            _data[i] += other._data[i];
        }

        return *this;
    }

    Matrix& operator-=(const Matrix& other)
    {
        for (std::size_t i = 0; i < R * C; i++) {
            _data[i] -= other._data[i];
        }

        return *this;
    }

    Matrix& operator*=(float other)
    {
        for (auto& elem : _data) {
            elem *= other;
        }

        return *this;
    }

    Matrix operator-()
    {
        Matrix m = *this;
        for (auto& elem : m._data) {
            elem = -elem;
        }

        return m;
    }

    static Matrix identity() requires square<R, C>
    {
        Matrix m;
        for (std::size_t i = 0; i < R; i++) {
            m(i, i) = 1.0f;
        }

        return m;
    }
};

template<std::size_t R, std::size_t RC, std::size_t C>
float operator*(const Row<R, RC>& row, const Column<RC, C>& col)
{
    float result = 0.0f;
    for (std::size_t i = 0; i < RC; i++) {
        result += row[i] * col[i];
    }

    return result;
}

template<std::size_t R, std::size_t RC, std::size_t C>
Matrix<R, C> operator*(const Matrix<R, RC>& left, const Matrix<RC, C>& right)
{
    Matrix<R, C> out;
    for (std::size_t r = 0; r < R; r++) {
        for (std::size_t c = 0; c < C; c++) {
            out(r, c) = left.row(r) * right.column(c);
        }
    }

    return out;
}

/**
 * Write a RxC matrix to an ostream.
 *
 * This function formats the output in a pleasing manner, where the
 * matrix is enclosed by vertical parentheses and the elements are
 * horizontally left-aligned.
 *
 * Example output:
 *              / 1 0  0 \
 * Matrix<3, 3> | 0 17 0 |
 *              \ 0 23 1 /
 */
template <std::size_t R, std::size_t C>
std::ostream& operator<<(std::ostream& out, const Matrix<R, C>& matrix)
{
    // Compute the type name and a left padding for remaining rows.
    std::ostringstream type_ss;
    type_ss << "Matrix<" << R << ", " << C << "> ";
    std::string type = type_ss.str();
    std::string prepad(type.size(), ' ');

    // Store formatted elements in a transposed array of arrays.
    std::array<std::array<std::string, R>, C> columns;

    for (std::size_t r = 0; r < R; r++) {
        for (std::size_t c = 0; c < C; c++) {
            std::ostringstream elem_ss;
            elem_ss << matrix(r, c);
            columns[c][r] = elem_ss.str();
        }
    }

    // Find the widest element for all columns
    auto comp = [](const std::string& a, const std::string& b) {
        return a.size() < b.size();
    };

    std::array<std::size_t, C> widths;

    for (std::size_t c = 0; c < C; c++) {
        auto max = std::max_element(columns[c].begin(), columns[c].end(), comp);
        std::size_t max_width = max->size();
        widths[c] = max_width;
    }

    // Compute the large parentheses.
    // Right paren is upside down left paren.
    std::string paren;
    if (R == 1) {
        paren = "|";
    } else {
        paren = "/" + std::string(R - 2, '|') + "\\";
    }

    for (std::size_t r = 0; r < R; r++) {
        // The type info is outputed rougly in the vertical center
        if (r == R / 2) {
            out << type;
        } else {
            out << prepad;
        }

        out << paren[r] << " ";

        for (std::size_t c = 0; c < C; c++) {
            const std::string& elem = columns[c][r];
            std::string pad(widths[c] - elem.size() + 1, ' ');
            out << elem << pad;
        }

        out << paren[R - r - 1] << std::endl;
    }

    return out;
}

#endif /* WFALL_MATRIX_H */
