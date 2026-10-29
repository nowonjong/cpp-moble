#pragma once
#include <filesystem>
#include <iosfwd>

using namespace std;

// 6번: 메뉴와 각 모듈의 연결. 테스트에서는 문자열 스트림을 전달한다.
int runApplication(const filesystem::path& dataDirectory,
                   istream& input, ostream& output, unsigned seed);

