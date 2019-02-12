#include "gltf.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace knu
{
	namespace graphics
	{
		using json = nlohmann::json;

		class gltf::impl
		{
		public:
			impl() {}
			impl(std::string_view gltf_file) { open_gltf_file(gltf_file); }

			void load(std::string_view gltf_file) { open_gltf_file(gltf_file); }
			bool has_node(std::string_view node_name)
			{
				auto iter = find_node(node_name);
				return iter != std::end(nodes_vec);
			}

			void build_node(std::string_view node_name, gltf_node& node)
			{
				auto node_iter = find_node(node_name);

				if (node_iter != std::end(nodes_vec))
				{
					node.node_name = node_name;
					load_glft_transformation(node_iter, node);

					if (node_iter->has_mesh)
						load_gltf_mesh(meshes_vec[node_iter->mesh_index], node);
				}
			}

		private:
			enum component_type {
				BYTE = 5120, UBYTE = 5121, SHORT = 5122, USHORT = 5123, UINT = 5125,
				FLOAT = 5126
			};
			enum data_type { SCALAR, VEC2, VEC3, VEC4, MAT2, MAT3, MAT4 };
			enum buffer_target { ARRAY_BUFFER = 34962, ELEMENT_ARRAY_BUFFER = 34963 };
			enum rendering_mode {
				POINTS, LINES, LINE_LOOP, LINE_STRIP, TRIANGLES,
				TRIANGLE_STRIP, TRIANGLE_FAN
			};		// Default is 4, Triangles
// rendering_mode perfectly matches the values of GL_POINTS, GL_LINES and so on

		private:
			struct accessors_struct
			{
				std::size_t buffer_view_ref;
				std::size_t byte_offset;	// may not be specified, so use 0
				component_type c_type;	// common component types BYTE = 5120, SHORT = 5122, FLOAT = 5126
				std::size_t count;
				// min/max components should go here, but not sure how to because
				//data could be either integer or floating point. Probably easier to do as floating
				// we'll leave it alone for now
				bool has_min_max = false;
				std::array<double, 3> min_bounds;
				std::array<double, 3> max_bounds;
				data_type d_type;
			};

			struct asset_struct
			{
				std::string generator;
				std::string version;
			};

			struct buffers_struct
			{
				std::size_t byte_length;
				std::string uri;
				std::vector<std::uint8_t> data;
			};

			struct buffer_views_struct
			{
				std::uint32_t buffer_index;
				std::size_t byte_length;
				std::uint32_t byte_offset;
				buffer_target target;
			};

			struct pbr_metallic_roughness_struct
			{
				std::array<double, 4> base_color_factor;
				double metallic_factor;
				double roughness_factor;
			};

			struct materials_struct
			{
				std::string material_name;
				pbr_metallic_roughness_struct material_roughness;
			};

			struct primitives_struct
			{
				bool has_position = false;
				bool has_normal = false;

				int position_index;
				int normal_index;

				int indices_ref;
				int materials_ref;
				rendering_mode render_mode;		// POINTS, LINES, TRIANGLES 
			};

			struct meshes_struct
			{
				std::string mesh_name;
				std::vector<primitives_struct> primitives_vec;
			};

			struct nodes_struct
			{
				std::string node_name;
				bool has_mesh = false;
				int mesh_index = -1;
				bool has_rotation = false;
				double rotation[4] = { 0.0, 0.0, 0.0, 1.0 };	// unit quaternion in [vec3, w]
				bool has_translation = false;
				double translation[3] = { 0.0, 0.0, 0.0 };
				bool has_scale = false;
				double scale[3] = { 1.0, 1.0, 1.0 };
			};

		private:

			// utility functions
			std::vector<nodes_struct>::iterator find_node(std::string_view node_name)
			{
				return std::find_if(std::begin(nodes_vec), std::end(nodes_vec),
					[=](const nodes_struct & n) {return n.node_name == node_name; });
			}

			std::vector<std::uint16_t> get_indices(const primitives_struct& ps)
			{
				const int accessor_index = ps.indices_ref;

				// for indices, c_type should be USHORT (5123)
				assert(USHORT == accessors_vec[accessor_index].c_type);

				const std::uint32_t indices_count = accessors_vec[accessor_index].count;
				const std::int32_t buffer_view_index = accessors_vec[accessor_index].buffer_view_ref;
				const std::uint32_t buffer_index = buffer_views_vec[buffer_view_index].buffer_index;

				// get the accessor byte offset
				const std::uint32_t accessor_byte_offset = accessors_vec[accessor_index].byte_offset;

				// get the buffer_view byte offset
				const std::uint32_t buffer_views_byte_offset = buffer_views_vec[buffer_view_index].byte_offset;

				const std::uint32_t offset = accessor_byte_offset + buffer_views_byte_offset;

				std::uint8_t * base_ptr = buffers_vec[buffer_index].data.data();
				std::uint16_t * start = reinterpret_cast<std::uint16_t*>(base_ptr);
				std::uint16_t * last = start + indices_count;

				return std::vector<std::uint16_t>{start, last};
			}

			gltf_component_info get_component_info(std::uint32_t accessor_index)
			{
				const accessors_struct& accessor_ref = accessors_vec[accessor_index];
				const buffer_views_struct& buffer_views_ref = buffer_views_vec[accessor_ref.buffer_view_ref];

				const std::size_t buffer_index = buffer_views_ref.buffer_index;
				const std::size_t byte_offset = accessor_ref.byte_offset + buffer_views_ref.byte_offset;
				const std::uint32_t component_type = accessor_ref.c_type;

				std::array<double, 3> min_bounds = { 0, 0, 0 };
				std::array<double, 3> max_bounds = { 0, 0, 0 };

				if (accessor_ref.has_min_max)
				{
					min_bounds = accessor_ref.min_bounds;
					max_bounds = accessor_ref.max_bounds;
				}

				std::size_t component_count = 0;
				switch (accessor_ref.d_type)
				{
				case data_type::VEC2:	component_count = 2; break;
				case data_type::VEC3:	component_count = 3; break;
				case data_type::VEC4:	component_count = 4; break;
				default: {
					// what????!!
					throw std::runtime_error("Unsupported data type specified");
				}break;
				}

				gltf_component_info info;
				info.buffer_index = buffer_index;
				info.byte_offset = byte_offset;
				info.component_count = component_count;
				info.component_type = component_type;
				info.min_bounds = min_bounds;
				info.max_bounds = max_bounds;
				info.valid = true;	// confirm that the info here is good

				return info;
			}

		private:
			std::vector<accessors_struct> accessors_vec;
			asset_struct asset;
			std::vector<buffers_struct> buffers_vec;
			std::vector<buffer_views_struct> buffer_views_vec;
			std::vector<materials_struct> materials_vec;
			std::vector<meshes_struct> meshes_vec;
			std::vector<nodes_struct> nodes_vec;
			std::string model_file_str;

		private:

			void open_gltf_file(std::string_view gltf_file)
			{
				json j = load_file(gltf_file);

				// store the file name and its path for use
				// in the GLTF_BUFFER case
				model_file_str = gltf_file;
				parse_json(j);
			}

			json load_file(std::string_view gltf_file)
			{
				std::ifstream file{ std::string(gltf_file) };
				if (!file) throw std::runtime_error{ "Unable to open file: "
					+ std::string(gltf_file) };

				json j;
				file >> j;

				return j;
			}

			void parse_json(json j)
			{
				const std::string GLTF_ACCESSORS = "accessors";
				const std::string GLTF_ASSET = "asset";
				const std::string GLTF_BUFFER_VIEWS = "bufferViews";
				const std::string GLTF_BUFFERS = "buffers";
				const std::string GLTF_MATERIALS = "materials";
				const std::string GLTF_MESHES = "meshes";
				const std::string GLTF_NODES = "nodes";

				json::iterator iter = std::begin(j);
				json::iterator last = std::end(j);

				while (iter != last)
				{
					const std::string current_key = iter.key();

					if (GLTF_ACCESSORS == current_key) { parse_accessors(iter.value()); }
					if (GLTF_ASSET == current_key) { parse_asset(iter.value()); }
					if (GLTF_BUFFER_VIEWS == current_key) { parse_buffer_views(iter.value()); }
					if (GLTF_BUFFERS == current_key) { parse_buffers(iter.value()); }
					if (GLTF_MATERIALS == current_key) { parse_materials2(iter.value()); }
					if (GLTF_MESHES == current_key) { parse_meshes2(iter.value()); }
					if (GLTF_NODES == current_key) { parse_nodes(iter.value()); }

					++iter;
				}
			}

			void parse_accessors(json::value_type val)
			{
				// accessors is an array of objects
				json::iterator iter = std::begin(val);
				json::iterator last = std::end(val);

				const std::string buffer_view_key = "bufferView";
				const std::string byte_offset_key = "byteOffset";
				const std::string component_type_key = "componentType";
				const std::string count_key = "count";
				// again, min and max variable go here, but for now, this will be unused
				const std::string min_key = "min";
				const std::string max_key = "max";
				const std::string type_key = "type";

				accessors_struct accessors;

				// for reference
				/*json::iterator scale_iter = b->find(scale_key);
				if (scale_iter != b->end())
				{
					nodes_vec.back().has_scale = true;
					json::value_type val = scale_iter.value();
					double a = val[0];
					double b = val[1];
					double c = val[2];
				*/
				while (iter != last)
				{
					memset(&accessors, 0, sizeof(accessors));
					auto bv = iter->value(buffer_view_key, static_cast<std::size_t>(-1));

					// 0 for default byte offset as the key may not be specified in the 
					// the gltf file for the accessor key
					auto boff = iter->value(byte_offset_key, static_cast<std::size_t>(0));
					auto ct = iter->value(component_type_key, 0);
					auto count = iter->value(count_key, static_cast<std::size_t>(0));
					auto dt = iter->value(type_key, "");

					accessors.buffer_view_ref = bv;
					accessors.byte_offset = boff;
					accessors.count = count;

					switch (ct)
					{
					case BYTE: accessors.c_type = BYTE; break;
					case UBYTE: accessors.c_type = UBYTE; break;
					case SHORT: accessors.c_type = SHORT; break;
					case USHORT: accessors.c_type = USHORT; break;
					case UINT: accessors.c_type = UINT; break;
					case FLOAT: accessors.c_type = FLOAT; break;
					}

					if ("SCALAR" == dt) accessors.d_type = SCALAR;
					if ("VEC2" == dt) accessors.d_type = VEC2;
					if ("VEC3" == dt) accessors.d_type = VEC3;
					if ("VEC4" == dt) accessors.d_type = VEC4;
					if ("MAT2" == dt) accessors.d_type = MAT2;
					if ("MAT3" == dt) accessors.d_type = MAT3;
					if ("MAT4" == dt) accessors.d_type = MAT4;


					json::iterator min_bound_iter = iter->find(min_key);

					// this is bad and need to be changed,
					// should not just be compared against SCALAR
					if (min_bound_iter != iter->end() && accessors.d_type != SCALAR)
					{
						accessors.has_min_max = true;
						json::value_type min_val = min_bound_iter.value();
						accessors.min_bounds = { min_val[0], min_val[1], min_val[2] };

						// there should be a max key
						json::iterator max_bound_iter = iter->find(max_key);

						if (max_bound_iter != iter->end())
						{
							json::value_type max_val = max_bound_iter.value();
							accessors.max_bounds = { max_val[0], max_val[1], max_val[2] };
						}
					}

					accessors_vec.emplace_back(accessors);

					++iter;
				}
			}

			void parse_asset(json::value_type val)
			{
				const std::string generator_key = "generator";
				const std::string version_key = "version";

				auto g = val.value(generator_key, "");
				auto v = val.value(version_key, "");
				asset = asset_struct{ g, v };
			}

			void parse_buffer_views(json::value_type val)
			{
				auto iter = std::begin(val);
				auto last = std::end(val);

				const std::string buffer_key = "buffer";
				const std::string byte_length_key = "byteLength";
				const std::string byte_offset_key = "byteOffset";
				const std::string target_key = "target";

				while (iter != last)
				{
					auto buf = iter->value(buffer_key, std::uint32_t(0));
					auto bl = iter->value(byte_length_key, std::size_t(0));
					auto bo = iter->value(byte_offset_key, std::uint32_t(0));
					auto tar = iter->value(target_key, int(0));

					buffer_views_struct bvs;

					switch (tar)
					{
					case ARRAY_BUFFER: bvs.target = ARRAY_BUFFER; break;
					case ELEMENT_ARRAY_BUFFER: bvs.target = ELEMENT_ARRAY_BUFFER; break;
					}

					bvs.buffer_index = buf;
					bvs.byte_length = bl;
					bvs.byte_offset = bo;

					buffer_views_vec.emplace_back(bvs);

					++iter;
				}
			}

			void parse_buffers(json::value_type val)
			{
				auto first_iter = std::begin(val);
				auto end_iter = std::end(val);

				const std::string byte_length_key{ "byteLength" };
				const std::string uri_key = "uri";

				while (first_iter != end_iter)
				{
					auto byte_length = first_iter->value(byte_length_key,
						static_cast<std::size_t>(0));

					auto uri = first_iter->value(uri_key, "");

					buffers_struct bs{ byte_length, uri };
					buffers_vec.emplace_back(bs);

					++first_iter;
				}

				// load the data for each buffer
				auto buffers_begin = std::begin(buffers_vec);
				auto buffers_end = std::end(buffers_vec);

				namespace fs = std::filesystem;

				fs::path model_path{ model_file_str };

				std::string separator;
				if (model_path.has_parent_path())
					separator = fs::path::preferred_separator;

				std::string parent_path;
				if (model_path.has_parent_path())
					parent_path = model_path.parent_path().string();

				// with this code, we're adding the constraint that .gltf files will be in the
				// same directory as the .bin files
				std::string path = parent_path + separator;

				// above code needs to be reworked
				// a path should be created for the uri
				// it should then be checked to see if it exists
				// if it doesn't, then the path leading to the model should
				// be used. If that doesn't work, then I don't know.

				while (buffers_begin != buffers_end)
				{
					std::ifstream file(path + buffers_begin->uri, std::ios::binary);

					if (!file)
						throw std::runtime_error("Unable to open file: " + buffers_begin->uri);

					std::vector<std::uint8_t> data(buffers_begin->byte_length);
					file.read(reinterpret_cast<char*>(&data[0]), data.size());
					buffers_begin->data = std::move(data);

					++buffers_begin;
				}
			}

			void parse_materials2(json::value_type val)
			{
				auto materials_iter_begin = std::begin(val);
				auto materials_iter_end = std::end(val);

				const std::string name_key = "name";
				const std::string pbr_metallic_roughness_key = "pbrMetallicRoughness";
				const std::string base_color_factor_key = "baseColorFactor";
				const std::string metallic_factor_key = "metallicFactor";
				const std::string roughness_factor_key = "roughnessFactor";

				while (materials_iter_begin != materials_iter_end)
				{
					materials_vec.emplace_back(materials_struct{});
					materials_struct& materials_ref = materials_vec.back();

					materials_ref.material_name = materials_iter_begin->value(name_key, "");

					json::iterator pbr_iter = materials_iter_begin->find(pbr_metallic_roughness_key);

					if (pbr_iter != materials_iter_begin->end())
					{
						// with pbr arm
						materials_ref.material_roughness.metallic_factor = pbr_iter->value(metallic_factor_key,
							0.0);
						materials_ref.material_roughness.roughness_factor = pbr_iter->value(
							roughness_factor_key, 0.0);

						json::iterator base_color_factor = pbr_iter->find(base_color_factor_key);

						if (base_color_factor != pbr_iter->end())
						{
							json::value_type bc_val = base_color_factor.value();
							std::array<double, 4> a = { bc_val[0],bc_val[1], bc_val[2], bc_val[3] };

							std::copy(std::begin(a), std::end(a),
								std::begin(materials_ref.material_roughness.base_color_factor));
						}
					}

					++materials_iter_begin;
				}
			}

			/*void parse_materials(json::value_type val)
			{
				// the following is hideous code, but it's the only solution I came up with.
				//
				std::cout << std::boolalpha << "is materials arm an array: " << val.is_array() << "\n";

				auto first_iter = std::begin(val);
				auto end_iter = std::end(val);

				const std::string name_key = "name";
				const std::string pbr_metallic_roughness_key = "pbrMetallicRoughness";
				const std::string base_color_factor_key = "baseColorFactor";
				const std::string metallic_factor_key = "metallicFactor";
				const std::string roughness_factor_key = "roughnessFactor";

				while (first_iter != end_iter)
				{
					auto n = first_iter->value(name_key, "");
					auto material_properties = first_iter->find(pbr_metallic_roughness_key);

					std::cout << "Is mat props is an object: " << std::boolalpha << material_properties->is_object();

					auto mat_begin = material_properties->begin();

					materials_struct ms;

					ms.material_name = n;
					while (mat_begin != material_properties->end())
					{
						auto current_key = mat_begin.key();
						auto val_array = mat_begin.value();

						std::cout << "val array is array: " << std::boolalpha << val_array.is_array() <<
							"count: " << val_array.size() << "\n";

						if (current_key == base_color_factor_key)
						{
							auto color_iter = std::begin(val_array);
							double zeroth = *color_iter; ++color_iter;
							double first = *color_iter; ++color_iter;
							double second = *color_iter; ++color_iter;
							double third = *color_iter; ++color_iter;

							ms.base_color_factor[0] = zeroth; ms.base_color_factor[1] = first;
							ms.base_color_factor[2] = second; ms.base_color_factor[3] = third;
						}

						if (current_key == metallic_factor_key)
						{
							double metallic_f = mat_begin.value();
							ms.metallic_factor = metallic_f;
						}

						if (current_key == roughness_factor_key)
						{
							double roughness_f = mat_begin.value();
							ms.roughness_factor = roughness_f;
						}

						++mat_begin;
					}

					materials_vec.emplace_back(ms);
					memset(&ms, 0, sizeof(materials_struct));

					++first_iter;
				}
			}*/

			void parse_meshes2(json::value_type val)
			{
				const std::string name_key = "name";
				const std::string mode_key = "mode";
				const std::string primitives_key = "primitives";
				const std::string attributes_key = "attributes";
				const std::string indices_key = "indices";
				const std::string materials_key = "material";
				const std::string normal_key = "NORMAL";
				const std::string position_key = "POSITION";
				const std::string texture_key = "TEXTURE";

				const int no_value = -1;

				json::iterator mesh_iter_begin = val.begin();
				json::iterator mesh_iter_end = val.end();

				while (mesh_iter_begin != mesh_iter_end)
				{
					meshes_vec.emplace_back(meshes_struct{});
					meshes_struct& meshes_ref = meshes_vec.back();

					meshes_ref.mesh_name = mesh_iter_begin->value(name_key, "");

					json::iterator primitives = mesh_iter_begin->find(primitives_key);

					auto primitives_iter_begin = primitives->begin();
					auto primitives_iter_end = primitives->end();

					while (primitives_iter_begin != primitives_iter_end)
					{
						meshes_ref.primitives_vec.emplace_back(primitives_struct{});
						primitives_struct& primitives_ref = meshes_ref.primitives_vec.back();

						primitives_ref.indices_ref = primitives_iter_begin->value(indices_key, no_value);
						int render_type = primitives_iter_begin->value(mode_key, TRIANGLES); // if no mode key
						// is specified, 4 (TRIANGLES) is the default renderering mode

						switch (render_type)
						{
						case 0: primitives_ref.render_mode = POINTS; break;
						case 1: primitives_ref.render_mode = LINES; break;
						case 2: primitives_ref.render_mode = LINE_LOOP; break;
						case 3: primitives_ref.render_mode = LINE_STRIP; break;
						case 4: primitives_ref.render_mode = TRIANGLES; break;
						case 5: primitives_ref.render_mode = TRIANGLE_STRIP; break;
						case 6: primitives_ref.render_mode = TRIANGLE_FAN; break;
						}

						primitives_ref.materials_ref = primitives_iter_begin->value(materials_key, no_value);

						json::iterator attributes_iter = primitives_iter_begin->find(attributes_key);

						if (attributes_iter != primitives_iter_begin->end())
						{
							primitives_ref.position_index = attributes_iter->value(position_key, no_value);

							if (primitives_ref.position_index != no_value)
								primitives_ref.has_position = true;

							primitives_ref.normal_index = attributes_iter->value(normal_key, no_value);

							if (primitives_ref.normal_index != no_value)
								primitives_ref.has_normal = true;

							// I guess here would be the setup for texture coordinates

						}

						++primitives_iter_begin;
					}

					++mesh_iter_begin;
				}
			}

			/*void parse_meshes(json::value_type val)
			{

				std::cout << "meshes is array: " << std::boolalpha << val.is_array() << "\n";
				std::cout << "meshes array size is: " << val.size() << "\n";

				std::vector<meshes_struct> vms;

				const std::string primitives_key = "primitives";
				const std::string attributes_key = "attributes";
				const std::string indices_key = "indices";
				const std::string materials_key = "material";
				const std::string normal_key = "NORMAL";
				const std::string position_key = "POSITION";
				const std::string texture_key = "TEXTURE";

				for (std::size_t i = 0; i < val.size(); ++i)
				{
					vms.emplace_back(meshes_struct{});

					std::string mesh_name = val[i].value("name", "");
					vms.back().mesh_name = mesh_name;

					json::iterator primitives = val[i].find(primitives_key);

					auto b = primitives->begin();
					auto e = primitives->end();

					while (b != e)
					{
						vms.back().primitives_vec.emplace_back(primitives_struct{});
						primitives_struct &p = vms.back().primitives_vec.back();

						std::cout << "Found primitves arm\n";
						std::cout << "Primitives are an array: " << std::boolalpha << b->is_array() << "\n";
						std::cout << "Primitives size: " << b->size() << "\n";
						auto attrib_obj = b->find(attributes_key);

						std::cout << *attrib_obj << "\n";

						if (auto s = attrib_obj->size(); s != 0)
						{
							auto normal_idx = attrib_obj->value(normal_key, 0);
							auto position_idx = attrib_obj->value(position_key, 0);

							p.has_position = true;
							p.has_normal = true;

							p.position_index = position_idx;
							p.normal_index = normal_idx;

						}

						auto indices_ref = b->value(indices_key, 0);
						auto materials_ref = b->value(materials_key, 0);

						p.indices_ref = indices_ref;
						p.materials_ref = materials_ref;

						++b;
					}
				}

				meshes_vec = vms;
			}*/

			void parse_nodes(json::value_type val)
			{
				if (val.is_array())
				{
					//std::cout << "nodes is an array\n";
				}
				else return;

				json::iterator b = val.begin();
				json::iterator e = val.end();

				const std::string name_key = "name";
				const std::string mesh_key = "mesh";
				const std::string rotation_key = "rotation";
				const std::string translation_key = "translation";
				const std::string scale_key = "scale";
				const int no_mesh_val = -1;		// to represent this node has no mesh

				while (b != e)
				{
					nodes_vec.push_back(nodes_struct{});

					std::string node_name = b->value(name_key, "");
					nodes_vec.back().node_name = node_name;

					int mesh_index = b->value(mesh_key, no_mesh_val);
					if (no_mesh_val != mesh_index)
					{
						// has a mesh
						nodes_vec.back().has_mesh = true;
						nodes_vec.back().mesh_index = mesh_index;
					}

					// check if node has rotation
					json::iterator rot_iter = b->find(rotation_key);
					if (rot_iter != b->end())
					{
						nodes_vec.back().has_rotation = true;
						json::value_type val = rot_iter.value();
						double a = val[0];
						double b = val[1];
						double c = val[2];
						double d = val[3];

						nodes_vec.back().rotation[0] = a; nodes_vec.back().rotation[1] = b;
						nodes_vec.back().rotation[2] = c; nodes_vec.back().rotation[3] = d;
					}

					// if scaling is present
					json::iterator scale_iter = b->find(scale_key);
					if (scale_iter != b->end())
					{
						nodes_vec.back().has_scale = true;
						json::value_type val = scale_iter.value();
						const double a = val[0];
						const double b = val[1];
						const double c = val[2];

						nodes_vec.back().scale[0] = a; nodes_vec.back().scale[1] = b;
						nodes_vec.back().scale[2] = c;
					}

					// if translation is present
					json::iterator translation_iter = b->find(translation_key);
					if (translation_iter != b->end())
					{
						nodes_vec.back().has_translation = true;
						json::value_type val = translation_iter.value();
						double a = val[0];
						double b = val[1];
						double c = val[2];

						nodes_vec.back().translation[0] = a; nodes_vec.back().translation[1] = b;
						nodes_vec.back().translation[2] = c;
					}

					// advance the iterator
					++b;
				}
			}

		private:
			// the gltf functions for building an object go here
			void load_glft_transformation(std::vector<nodes_struct>::iterator iter, gltf_node & node)
			{
				if (iter->has_scale)
				{
					node.scale[0] = iter->scale[0];
					node.scale[1] = iter->scale[1];
					node.scale[2] = iter->scale[2];
				}
				else
				{
					node.scale[0] = 1.0f;
					node.scale[1] = 1.0f;
					node.scale[2] = 1.0f;
				}

				if (iter->has_translation)
				{
					node.translation[0] = iter->translation[0];
					node.translation[1] = iter->translation[1];
					node.translation[2] = iter->translation[1];
				}
				else
				{
					node.translation[0] = 0.0;
					node.translation[1] = 0.0;
					node.translation[2] = 0.0;
				}

				if (iter->has_rotation)
				{
					// unit quaternion in form [vec3, w]
					node.rotation[0] = iter->rotation[0];
					node.rotation[1] = iter->rotation[1];
					node.rotation[2] = iter->rotation[2];
					node.rotation[3] = iter->rotation[3];
				}
				else
				{
					// unit quaternion in form [vec3, w]
					node.rotation[0] = 0.0; // x
					node.rotation[1] = 0.0;	// y
					node.rotation[2] = 0.0;	// z
					node.rotation[3] = 1.0;	// w
				}
			}

			void load_gltf_mesh(const meshes_struct & m, gltf_node & node)
			{
				node.mesh.mesh_name = m.mesh_name;

				// load all the materials
				std::for_each(std::begin(materials_vec), std::end(materials_vec),
					[&node](const materials_struct & ms)
				{
					node.mesh.materials.emplace_back(gltf_material{});
					gltf_material& mat_ref = node.mesh.materials.back();

					mat_ref.material_name = ms.material_name;
					mat_ref.base_color_factor = ms.material_roughness.base_color_factor;
					mat_ref.metallic_factor = ms.material_roughness.metallic_factor;
					mat_ref.roughness_factor = ms.material_roughness.roughness_factor;
				});

				// copy all the buffers
				std::for_each(std::begin(buffers_vec), std::end(buffers_vec),
					[&node](const buffers_struct & bs)
				{
					node.mesh.buffers.emplace_back(gltf_buffer{ bs.byte_length,
						bs.data });
				});

				for (auto primitive = std::begin(m.primitives_vec); primitive != std::end(m.primitives_vec); ++primitive)
				{
					std::vector<std::uint16_t> indices = get_indices(*primitive);
					node.mesh.sub_meshes.emplace_back(gltf_partial_mesh{});
					gltf_partial_mesh& sub_mesh_ref = node.mesh.sub_meshes.back();

					gltf_component_info position_info;
					if (primitive->has_position)
						position_info = get_component_info(primitive->position_index);

					gltf_component_info normal_info;
					if (primitive->has_normal)
						normal_info = get_component_info(primitive->normal_index);

					std::uint32_t material_index = primitive->materials_ref;

					// future, texture coordinates

					sub_mesh_ref.render_mode = primitive->render_mode;
					sub_mesh_ref.material_index = material_index;
					sub_mesh_ref.indices = indices;
					sub_mesh_ref.position_info = position_info;
					sub_mesh_ref.normal_info = normal_info;
				}
			}
		};

		gltf::gltf() :
			impl_ptr{ std::make_unique<impl>() }
		{}
		gltf::gltf(std::string_view gltf_file) :
			impl_ptr{ std::make_unique<impl>(gltf_file) }
		{}

		gltf::~gltf() = default;

		void gltf::load(std::string gltf_file)
		{
			impl_ptr->load(gltf_file);
		}

		bool gltf::has_node(std::string_view node_name)
		{
			return impl_ptr->has_node(node_name);
		}

		std::pair<bool, gltf_node> gltf::build_node(std::string_view node_name)
		{
			gltf_node node;
			if (!has_node(node_name))
				return { false, node };

			impl_ptr->build_node(node_name, node);;
			return { true, node };
		}

		std::pair<bool, gltf_node> load_gltf_node(std::string_view model_name,
			std::string_view node_name)
		{
			using namespace std::filesystem;

			gltf_node node;
			bool success = false;

			if (!exists(path{ model_name }))
				return { false, node };

			try {
				gltf model(model_name);
				if (!model.has_node(node_name))
					return { false, gltf_node{} };

				std::tie(success, node) = model.build_node(node_name);
			}
			catch (std::runtime_error & e)
			{
				std::cerr << e.what() << "\n";
				return { false, gltf_node{} };
			}

			return { success, node };
		}
	} // namespace graphics
} // namespace knu
