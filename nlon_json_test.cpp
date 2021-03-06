// nlon_json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//#include "json.hpp"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "gltf.hpp"


//using json = nlohmann::json;
const std::string file_name{ "box.gltf" };
const std::string path{ "models/" };
std::string node_name = "Cube";

using namespace std;

int main()
{
	//knu::graphics::gltf box(file_name);

	bool success;
	knu::graphics::gltf_node cube_node;
	//cout << boolalpha << box.has_node(node_name) << "\n";

	std::tie(success, cube_node) =
		knu::graphics::load_gltf_node(file_name, path, node_name);


	return 0;
}
