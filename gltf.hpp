#ifndef KNU_3DGLTF_HPP
#define KNU_3DGLTF_HPP

#include <string>
#include <memory>
#include <array>
#include <vector>


struct gltf_partial_mesh
{
	std::vector<std::uint16_t> indices;
};

struct gltf_buffer
{
	std::size_t byte_length;
	std::vector<std::uint8_t> data;
};
struct gltf_mesh
{
	std::string_view mesh_name;
	std::vector<gltf_partial_mesh> sub_meshes;
	std::vector<gltf_buffer> buffers;
};

struct gltf_node
{
	std::string_view node_name;
	std::array<double, 3> scale;
	std::array<double, 3> translation;
	std::array<double, 4> rotation;		// unit quaternion in form [vec3, w]
	gltf_mesh mesh;
};

class gltf
{
	public:
		gltf();
		gltf(std::string gltf_file);
		~gltf();
		void load(std::string gltf_file);
		bool has_node(std::string_view node_name);
		void build_node(std::string_view node_name);

	private:
		class impl;
		std::unique_ptr<impl> impl_ptr;
};
#endif // !KNU_3DGLTF_HPP

