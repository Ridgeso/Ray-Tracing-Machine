#pragma once
#include <glm/glm.hpp>
#include <vector>
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
			float padding[1];
		};
		#pragma pack(pop)

	public:
		Camera(float fov, float near, float far);

		const glm::vec3& GetPosition() const { return spec.position; }
		glm::vec3& GetPosition() { return spec.position; }
		void SetPosition(const glm::vec3& newPos) { spec.position = newPos; }

		const glm::vec3& GetDirection() const { return direction; }
		glm::vec3& GetDirection() { return direction; }
		void SetDirection(const glm::vec3& newDir) { direction = newDir; }

		const glm::mat4& GetInvProjection() const { return spec.invProjection; }
		const glm::mat4& GetInvView() const { return spec.invView; }

		const Spec& GetSpec() const { return spec; }

		void RecalculateInvProjection();
		void RecalculateInvView();

		bool ResizeCamera(int32_t width, int32_t height);

	private:
		Spec spec;
		glm::vec3 direction;

		float fov, nearPlane, farPlane;
		glm::ivec2 viewSize;

		static const glm::vec3 Up;
	};

}
