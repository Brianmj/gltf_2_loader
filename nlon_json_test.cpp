// nlon_json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//#include "json.hpp"
#include <iostream>
#include <iomanip>
#include "gltf.hpp"


//using json = nlohmann::json;
const std::string file_name{ "box.gltf" };
const std::string simple_mesh_name{ "mesh_primitives_00.gltf" };
std::string_view node = "Cube";

using namespace std;

int main()
{
	//gltf primitive{ simple_mesh_name };

	knu::gltf box(file_name);
	cout << boolalpha << box.has_node(node) << "\n";
	auto [success, n] = box.build_node(node);

	return 0;
}
