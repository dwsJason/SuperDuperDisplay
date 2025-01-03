#include "A2WindowBeam.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

A2WindowBeam::A2WindowBeam(A2VideoModeBeam_e _video_mode, const char* shaderVertexPath, const char* shaderFragmentPath)
{
	video_mode = _video_mode;
	shader = Shader();
	shader.build(shaderVertexPath, shaderFragmentPath);
}

A2WindowBeam::~A2WindowBeam()
{
	if (VRAMTEX != UINT_MAX)
		glDeleteTextures(1, &VRAMTEX);
	if (FBO != UINT_MAX)
	{
		glDeleteFramebuffers(1, &FBO);
		glDeleteTextures(1, &output_texture_id);
	}
	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void A2WindowBeam::SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath)
{
	this->shader.build(shaderVertexPath, shaderFragmentPath);
}


uint32_t A2WindowBeam::GetWidth() const
{
	return screen_count.x;
}

uint32_t A2WindowBeam::GetHeight() const
{
	return screen_count.y;
}

void A2WindowBeam::SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical)
{
	border_width_cycles = cycles_horizontal;
	border_height_scanlines = scanlines_vertical;
	uint32_t cycles_h_with_border = 40 + (2 * border_width_cycles);
	// Legacy is 14 dots per cycle, SHR is 16 dots per cycle
	// Multiply border size by 4 and not 2 because height is doubled
	switch (video_mode) {
	case A2VIDEOBEAM_SHR:
		screen_count = uXY({ cycles_h_with_border * 16 , 400 + (4 * border_height_scanlines) });
		break;
	default:	//e
		screen_count = uXY({ cycles_h_with_border * 14, 384 + (4 * border_height_scanlines) });
		break;
	}
	UpdateVertexArray();
}


void A2WindowBeam::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The A2WindowBeam always covers the whole screen, so from -1 to 1 on both axes
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(A2BeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2BeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
	vertices.push_back(A2BeamVertex({ glm::vec2(1,  1), glm::ivec2(screen_count.x, screen_count.y) }));	// top right
	vertices.push_back(A2BeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2BeamVertex({ glm::vec2(-1, -1), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(A2BeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
}


GLuint A2WindowBeam::GetOutputTextureId() const
{
	return output_texture_id;
}

GLuint A2WindowBeam::Render(bool shouldUpdateDataInGPU)
{
	// std::cerr << "Rendering " << (int)video_mode << " - " << shouldUpdateDataInGPU << std::endl;
	// std::cerr << "border w " << border_width_cycles << " - h " << border_height_scanlines << std::endl;
	if (!shader.isReady)
		return UINT32_MAX;
	if (vertices.size() == 0)
		return UINT32_MAX;

	GLenum glerr;
	if (VRAMTEX == UINT_MAX)
	{
		glGenTextures(1, &VRAMTEX);
		glActiveTexture(_TEXUNIT_DATABUFFER);
		glBindTexture(GL_TEXTURE_2D, VRAMTEX);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glActiveTexture(GL_TEXTURE0);
	}
	if (PAL256TEX == UINT_MAX)
	{
		glGenTextures(1, &PAL256TEX);
		glActiveTexture(_TEXUNIT_PAL256BUFFER);
		glBindTexture(GL_TEXTURE_2D, PAL256TEX);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glActiveTexture(GL_TEXTURE0);
	}

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	if (FBO == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glGenTextures(1, &output_texture_id);
		glActiveTexture(_TEXUNIT_POSTPROCESS);
		glBindTexture(GL_TEXTURE_2D, output_texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_count.x, screen_count.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_id, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		glActiveTexture(GL_TEXTURE0);
		// glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL render A2WindowBeam setup error: " << glerr << std::endl;
	}
	// std::cerr << "VRAMTEX " << VRAMTEX << " VAO " << VAO << " FBO " << FBO << std::endl;

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return UINT32_MAX;
	}
	
	glBindVertexArray(VAO);

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(A2BeamVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)offsetof(A2BeamVertex, PixelPos));
		
		// And set the borders
		shader.setInt("hborder", (int)border_width_cycles);
		shader.setInt("vborder", (int)border_height_scanlines);
	}

	// Associate the texture VRAMTEX in TEXUNIT_DATABUFFER with the buffer
	// This is the apple 2's memory which is mapped to a "texture"
	// Always update that buffer in the GPU
	if (shouldUpdateDataInGPU)
	{
		uint32_t cycles_w_with_border = 40 + (2 * border_width_cycles);
		glActiveTexture(_TEXUNIT_DATABUFFER);
		glBindTexture(GL_TEXTURE_2D, VRAMTEX);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "A2WindowBeam::Render 5 error: " << glerr << std::endl;
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		if (vramTextureExists)	// it exists, do a glTexSubImage2D() update
		{
			switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				// Adjust the unpack alignment for textures with arbitrary widths
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				// Don't update the interlace part if unnecessary
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _COLORBYTESOFFSET + (cycles_w_with_border * 4),
								(_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines)) * (interlaceSHRMode + 1),
								GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
				if (((specialModesMask & A2_VSM_SHR4PAL256) != 0) || (overrideSHR4Mode == 2))
				{
					glActiveTexture(_TEXUNIT_PAL256BUFFER);
					glBindTexture(GL_TEXTURE_2D, PAL256TEX);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _A2VIDEO_SHR_BYTES_PER_LINE, _A2VIDEO_SHR_SCANLINES * (interlaceSHRMode + 1),
									GL_RED_INTEGER, GL_UNSIGNED_SHORT, (uint16_t*)(A2VideoManager::GetInstance()->GetPAL256VRAMReadPtr()));
					glActiveTexture(_TEXUNIT_DATABUFFER);
				}
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				break;
			case A2VIDEOBEAM_FORCED_TEXT1:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_TEXT2:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT2VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR1:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR2:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR2VRAMReadPtr());
				break;
			default:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cycles_w_with_border, 192 + (2 * border_height_scanlines), GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
			}
		}
		else {	// texture doesn't exist, create it with glTexImage2D()
			switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				// Adjust the unpack alignment for textures with arbitrary widths
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				// the size of the SHR texture is Scanlines+2xBorderHeight, doubled for interlace
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, _COLORBYTESOFFSET + (cycles_w_with_border * 4),
							 (_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines)) * _INTERLACE_MULTIPLIER, 0,
							 GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
				// Create the PAL256TEX texture
				glActiveTexture(_TEXUNIT_PAL256BUFFER);
				glBindTexture(GL_TEXTURE_2D, PAL256TEX);
				// The size of the PAL256 texture is 2 bytes per SHR byte. Add the interlacing and that's 4x scanlines
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, _A2VIDEO_SHR_BYTES_PER_LINE,
							 2 * _A2VIDEO_SHR_SCANLINES * _INTERLACE_MULTIPLIER, 0,
							 GL_RED_INTEGER, GL_UNSIGNED_SHORT, (uint16_t*)(A2VideoManager::GetInstance()->GetPAL256VRAMReadPtr()));
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				break;
			case A2VIDEOBEAM_FORCED_TEXT1:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_TEXT2:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT2VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR1:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR2:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR2VRAMReadPtr());
				break;
			default:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, cycles_w_with_border, 192 + (2 * border_height_scanlines), 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
			}
			vramTextureExists = true;
		}
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam::Render error: " << glerr << std::endl;
	}

	shader.setInt("ticks", SDL_GetTicks());
	shader.setInt("specialModesMask", specialModesMask);
	shader.setInt("monitorColorType", monitorColorType);

	// point the uniform at the VRAM texture
	shader.setInt("VRAMTEX", _TEXUNIT_DATABUFFER - GL_TEXTURE0);
	
	// And set all the modes textures that the shader will use
	// 2 font textures + lgr, hgr, dhgr
	// as well as any other unique mode data
	if (video_mode == A2VIDEOBEAM_SHR)
	{
		shader.setInt("PAL256TEX", _TEXUNIT_PAL256BUFFER - GL_TEXTURE0);
		shader.setInt("overrideSHR4Mode", overrideSHR4Mode);
		shader.setInt("interlaceSHRMode", interlaceSHRMode);
		shader.setInt("interlaceSHRYOffset", interlaceSHRMode * (_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines)));
		shader.setInt("interlacePal256YOffset", interlaceSHRMode * (_A2VIDEO_SHR_SCANLINES));
	}
	else {
		shader.setInt("a2ModesTex0", _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0);	// D/TEXT font regular
		shader.setInt("a2ModesTex1", _TEXUNIT_IMAGE_ASSETS_START + 1 - GL_TEXTURE0);	// D/TEXT font alternate
		shader.setInt("a2ModesTex2", _TEXUNIT_IMAGE_ASSETS_START + 2 - GL_TEXTURE0);	// D/LGR
		shader.setInt("a2ModesTex3", _TEXUNIT_IMAGE_ASSETS_START + 3 - GL_TEXTURE0);	// HGR
		shader.setInt("a2ModesTex4", _TEXUNIT_IMAGE_ASSETS_START + 4 - GL_TEXTURE0);	// DHGR
	}
	
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam render error: " << glerr << std::endl;
	}
	return output_texture_id;
}
