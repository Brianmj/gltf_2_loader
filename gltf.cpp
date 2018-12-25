#include "gltf.hpp"
#include "json.hpp"

class gltf::impl
{
public:
	impl() {}
	impl(std::string gltf_file) { open_gltf_file(gltf_file); }

	void load(std::string gltf_file) { open_gltf_file(gltf_file); }

private:
	void open_gltf_file(std::string gltf_file)
	{

	}

};
		
gltf::gltf() : impl_ptr{ new impl } {}
gltf::gltf(std::string gltf_file) : impl_ptr{ new impl(gltf_file) } {}


	
void gltf::load(std::string file_name)
{
}

