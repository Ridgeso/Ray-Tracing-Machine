#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RT
{
	
	Camera::Camera(float fov, float nearPlane, float farPlane)
		: spec{}
		, fov(fov)
		, nearPlane(nearPlane)
		, farPlane(farPlane)
		, viewSize(0)
	{
		spec.position = glm::vec3(0, 1, 5);
		direction = glm::vec3(0, 0, -1);
		spec.invProjection = glm::mat4(0);
		spec.focusDistance = 1.0f;
		spec.defocusStrength = 0.0f;
		spec.blurStrength = 0.0f;

		recalculateInvView();
	}

	void Camera::recalculateInvProjection()
	{
		auto projection = glm::perspectiveFov(glm::radians(fov), (float)viewSize.x, (float)viewSize.y, nearPlane, farPlane);
		spec.invProjection = glm::inverse(projection);
	}

	void Camera::recalculateInvView()
	{
		auto view = glm::lookAt(spec.position, spec.position + direction, Up);
		spec.invView = glm::inverse(view);
	}

	bool Camera::resizeCamera(int32_t width, int32_t height)
	{
		if (viewSize.x == width && viewSize.y == height)
			return false;

		viewSize = { width, height };
		recalculateInvProjection();
		return true;
	}

}
