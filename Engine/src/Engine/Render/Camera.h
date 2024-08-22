#pragma once
#include <glm/glm.hpp>
#include <cstdint>

namespace RT
{

	class Camera
	{
	public:
		#pragma pack(push, 1)
		struct Spec
		{
			glm::mat4 invProjection;
			glm::mat4 invView;
			glm::vec3 position;
			float blurStrength;
		};
		#pragma pack(pop)

	public:
		Camera(float fov, float near, float far);

		const glm::vec3& getPosition() const { return spec.position; }
		glm::vec3& getPosition() { return spec.position; }

		const glm::vec3& getDirection() const { return direction; }
		glm::vec3& getDirection() { return direction; }

		const glm::mat4& getInvProjection() const { return spec.invProjection; }
		const glm::mat4& getInvView() const { return spec.invView; }

		const Spec& getSpec() const { return spec; }
		Spec& getSpec() { return spec; }

		void recalculateInvProjection();
		void recalculateInvView();

		bool resizeCamera(int32_t width, int32_t height);

	public:
		float fov, nearPlane, farPlane;

	private:
		Spec spec;
		glm::vec3 direction;

		glm::ivec2 viewSize;

		inline static constexpr glm::vec3 Up = glm::vec3(0, 1, 0);
	};

}
