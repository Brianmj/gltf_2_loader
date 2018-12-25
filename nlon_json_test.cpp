// nlon_json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "gltf.hpp"

using json = nlohmann::json;
const std::string file_name = "box.gltf";

using namespace std;

int main()
{
	ifstream file(file_name);

	if (!file) { cout << "Unable to open file\n"; }
	else { cout << "File successfully opened\n"; }

	json j;

	file >> j;

	//cout << j.dump(4) << "\n";

	for (auto f = j.begin(); f != j.end(); ++f)
	{
		if (f.key() == std::string("buffers"))
		{
			cout << f.key() << " ========== " << f.value() << "\n\n";
			auto arr = f.value();
			cout << arr.size() << "\n";
			cout << arr.type_name() << "\n";
			cout << boolalpha << "iter is an array? " << f->is_array() << "\n";

			auto b = arr.begin();
			auto e = arr.end();

			while (b != e)
			{
				cout << "is object: " << boolalpha << b->is_object() << "\n";
				int byte_size = b->value("byteLength", 0);
				std::string uri = b->value("uri", std::string());
			}
			
		}
	}
}
