
#pragma once
#include "glm/ext/vector_float3.hpp"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // rotate, lookAt, perspective

class CameraController {
        public:
	CameraController() {
		camera_.position = glm::vec3(2.0f, 2.0f, 2.0f);
		camera_.direction = glm::vec3(0.0f); // or target for look-at
		camera_.up = glm::vec3(0.0f, 1.0f, 0.0f);
		camera_.fov = 45.0f;
		camera_.aspectRatio = 0;
		camera_.nearPlane = 0.1f;
		camera_.farPlane = 10.0f;
	};

	struct View_Proj {
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct Camera {
		glm::vec3 position;
		glm::vec3 direction; // or target for look-at
		glm::vec3 up;
		float fov;
		float aspectRatio;
		float nearPlane;
		float farPlane;
	};

	void setAspectratio(float aspectRatio) {
		camera_.aspectRatio = aspectRatio;
	}

	void update() {
		// camera_ positioned at (2,2,2) looking at origin
		vp_.view = glm::lookAt(camera_.position,  // eye
				   camera_.direction, // center
				   camera_.up	  // up
		);

		vp_.proj = glm::perspective(
		  glm::radians(camera_.fov), camera_.aspectRatio, camera_.nearPlane,
		  camera_.farPlane); // Vulkan Y-axis is flipped vs OpenGL — fix it
		vp_.proj[1][1] *= -1;
	}

	View_Proj vp_;
	Camera camera_;
};
