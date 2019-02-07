#ifndef KNU_GLTF_HPP
#define KNU_GLTF_HPP

#include <string>
#include <memory>
#include <array>
#include <vector>

namespace knu
{
	// a struct containing information about the component (position, normal, texture coords)
	struct gltf_component_info
	{
		bool valid = false;				// use this flag to determine whether the rest of the struct is good to use
		std::uint32_t buffer_index;
		std::uint32_t byte_offset;
		std::uint32_t component_type;	// unsigned int, float (compatable with OpenGL's GL_UNSIGNED_INT, GL_FLOAT and so on)
		std::uint32_t component_count;	// should be 2 (tex), 3 (position, normal) or 4 (position)
		std::array<double, 3> min_bounds;
		std::array<double, 3> max_bounds;
	};

	struct gltf_partial_mesh
	{
		std::uint32_t render_mode;		// for first parameter of glDrawArrays(), GL_POINTS, GL_TRIANGLES
		std::uint32_t material_index;
		std::vector<std::uint16_t> indices;
		gltf_component_info position_info;
		gltf_component_info normal_info;
	};

	struct gltf_buffer
	{
		std::size_t byte_length;
		std::vector<std::uint8_t> data;
	};

	struct gltf_material
	{
		std::string material_name = "";
		std::array<double, 4> base_color_factor = { 0.0, 0.0, 0.0, 0.0 };
		double metallic_factor = 0.0;
		double roughness_factor = 0.0;
	};

	struct gltf_mesh
	{
		std::string_view mesh_name;
		std::vector<gltf_partial_mesh> sub_meshes;
		std::vector<gltf_buffer> buffers;
		std::vector<gltf_material> materials;
	};

	struct gltf_node
	{
		std::string_view node_name = {};
		std::array<double, 3> scale = { 0, 0, 0 };
		std::array<double, 3> translation = { 0, 0, 0 };
		std::array<double, 4> rotation = { 0, 0, 0, 1 }; // unit quaternion in form [vec3, w]
		gltf_mesh mesh;
	};

	class gltf
	{
	public:
		gltf();
		gltf(std::string_view gltf_file);
		// no copying, but moving is ok
		gltf(const gltf&) = delete;
		gltf& operator=(const gltf&) = delete;

		// moves
		gltf(gltf&&) = default;
		gltf& operator=(gltf&&) = default;

		~gltf();
		// ~gltf() = default; cannot define destructor here because
		// of pimpl idiom. Destructor has to be defined near the end
		// of the cpp file so that it can "see" the entire definition
		// of impl

		void load(std::string gltf_file);
		bool has_node(std::string_view node_name);
		std::pair<bool, gltf_node> build_node(std::string_view node_name);

	private:
		class impl;
		std::unique_ptr<impl> impl_ptr;
	};

	// a convenience function for loading a model with a node
	std::pair<bool, gltf_node> load_gltf_node(std::string_view model_name,
		std::string_view node_name);
}

#endif // !KNU_GLTF_HPP

