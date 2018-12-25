#ifndef KNU_3DGLTF_HPP
#define KNU_3DGLTF_HPP

#include <string>
#include <memory>


class gltf
{
	public:
		gltf();
		gltf(std::string gltf_file);
		void load(std::string gltf_file);

	private:
		class impl;
		std::unique_ptr<impl> impl_ptr;
};
#endif // !KNU_3DGLTF_HPP

