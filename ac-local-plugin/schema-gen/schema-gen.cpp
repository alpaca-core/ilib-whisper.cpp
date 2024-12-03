// Copyright (c) Alpaca Core
// SPDX-License-Identifier: MIT
//
#include <ac/schema/WhisperCpp.hpp>
#include <ac/schema/GenerateLoaderSchemaDict.hpp>
#include <iostream>

int main() {
    auto d = ac::local::schema::generateLoaderSchema<acnl::ordered_json, ac::local::schema::WhisperCppLoader>();
    std::cout << d.dump(2) << std::endl;
    return 0;
}
