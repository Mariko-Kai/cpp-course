
#pragma once
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <map>
#include <stdexcept>
#include <string>

template <typename T>
class SparseMatrixNew;

template <typename T>
class MatrixN_1DimProxy
{
    SparseMatrixNew<T> &matrix;
    std::unique_ptr<std::vector<uint64_t>> coords_ptr = std::make_unique<std::vector<uint64_t>>;

public:
    MatrixN_1DimProxy(SparseMatrixNew<T> &matrix, std::unique_ptr<std::vector<uint64_t>> coords) : matrix(matrix), coords_ptr(std::move(coords)) {}

    MatrixN_1DimProxy<T> operator[](uint64_t n_1_coord)
    {
        coords_ptr->push_back(n_1_coord);
        return MatrixN_1DimProxy<T>(matrix, std::move(coords_ptr));
    }
    MatrixN_1DimProxy<T> &operator=(T value)
    {
        matrix.addValue(value, *coords_ptr);
        return *this;
    }

    operator T() const
    {
        return matrix.getValue(*coords_ptr);
    }
};

template <typename T>
class SparseMatrixNew
{
    T default_value;
    std::map<std::vector<uint64_t>, T> values;
    size_t max_arity = 0;

public:
    SparseMatrixNew(T default_value) : default_value(default_value) {}

    MatrixN_1DimProxy<T> operator[](uint64_t n_coord)
    {
        auto coord = std::make_unique<std::vector<uint64_t>>();
        coord->push_back(n_coord);
        return MatrixN_1DimProxy<T>(*this, std::move(coord));
    }

    using iterator = typename std::map<std::vector<uint64_t>, T>::iterator;

    void addValue(T value, std::vector<uint64_t> coords)
    {
        if (max_arity > 0)
        {
            if (coords.size() != max_arity)
                throw std::invalid_argument(
                    "Invalid element arity: expected " + std::to_string(max_arity) +
                    ", got " + std::to_string(coords.size()));
        }
        else if (max_arity == 0)
        {
            max_arity = coords.size();
        }

        if (value == default_value)
        {
            if (values.find(coords) != values.end())
                values.erase(coords);
        }
        else
        {
            values[coords] = value;
        }
    }

    T getValue(std::vector<uint64_t> coords) const
    {
        auto it = values.find(coords);
        if (it != values.end())
            return it->second;
        return default_value;
    }

    void showMatrix() const
    {
        for (const auto &raw_data : values)
        {
            std::cout << raw_data.second << " ";
            for (const auto &coord : raw_data.first)
                std::cout << coord << " ";
            std::cout << std::endl;
        }
        std::cout << "\n";
    }

    int size() const
    {
        return values.size();
    }
    iterator begin()
    {
        return values.begin();
    }
    iterator end()
    {
        return values.end();
    }
};
