#include "svmMOBS.hpp"
#include "svmFromFile.hpp"
#include "svmShaderString.hpp"
#include "svmError.hpp"
#include "svmVBC.hpp"

sanMOBS::sanMOBS(sanXML* pxml, PM* ppm, sanCamera* pcameras, sanPGS* ppgs, VEHICLESIGNAL* pvehicleSignal)
{
	m_pxml = pxml;
	m_ppm = ppm;
	m_pcameras = pcameras;
	m_ppgs = ppgs;
	m_pvehicleSignal = pvehicleSignal;
	m_pmarkers = new MOBS_MARKERS();
}

sanMOBS::~sanMOBS()
{
	if (!m_center_pt3d_mm.empty()) m_center_pt3d_mm.clear(); else noop;
	if (!m_vehicle_heading_rad.empty())  m_vehicle_heading_rad.clear(); else noop;
	if (m_pmarkers != nullptr) delete m_pmarkers;
	m_mobsShader.~sanShader();
}

void sanMOBS::initialize()
{
	try
	{
		m_max_distance = MAX6(m_pxml->m_mobs_parameter.mois_dist_warning_head, m_pxml->m_mobs_parameter.mois_dist_monitoring_head,\
							  m_pxml->m_mobs_parameter.right_bsis_dist_warning_head, m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_head,\
							  m_pxml->m_mobs_parameter.left_bsis_dist_warning_head, m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_head);

		generate_trajectory(90.0f, m_max_distance);

		get_mois_guide(	m_pmarkers->mois_warning_pt3d_mm,\
						m_pmarkers->mois_monitoring_pt3d_mm);

		get_bsis_guide(	m_pmarkers->right_bsis_top_warning_pt3d_mm,\
						m_pmarkers->right_bsis_bot_warning_pt3d_mm,\
						m_pmarkers->right_bsis_top_monitoring_pt3d_mm,\
						m_pmarkers->right_bsis_mid_monitoring_pt3d_mm,\
						m_pmarkers->right_bsis_bot_monitoring_pt3d_mm,\
						m_pmarkers->left_bsis_top_warning_pt3d_mm,\
						m_pmarkers->left_bsis_bot_warning_pt3d_mm,\
						m_pmarkers->left_bsis_top_monitoring_pt3d_mm,\
						m_pmarkers->left_bsis_mid_monitoring_pt3d_mm,\
						m_pmarkers->left_bsis_bot_monitoring_pt3d_mm);

		get_rear_guide(	m_pmarkers->right_rear_warning_pt3d_mm, \
						m_pmarkers->left_rear_warning_pt3d_mm,\
						m_pmarkers->center_rear_warning_pt3d_mm);

	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2108001", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanMOBS::renderMOBS(int vabt_idx, glm::vec3 color, float alpha, glm::mat4 mvp)
{
	sanError::glClearError();

	m_mobsShader.use();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);
	glUniformMatrix4fv(glGetUniformLocation(m_mobsShader.getProgram(), "mvp"), 1, GL_FALSE, &mvp[0][0]);
	glUniform3fv(glGetUniformLocation(m_mobsShader.getProgram(), "color"), 1, &color[0]);

	glUniform1f(glGetUniformLocation(m_mobsShader.getProgram(), "alpha"), alpha);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vabt_list[vabt_idx].vnum);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}

void sanMOBS::drawLayout0(glm::mat4& view_matrix)
{
	try
	{
		if (m_pxml->m_activation.mobs)
		{
			generate_trajectory(90.0f, m_max_distance);

			get_mois_guide(	m_pmarkers->mois_warning_pt3d_mm, \
							m_pmarkers->mois_monitoring_pt3d_mm);

			get_bsis_guide(	m_pmarkers->right_bsis_top_warning_pt3d_mm,\
							m_pmarkers->right_bsis_bot_warning_pt3d_mm,\
							m_pmarkers->right_bsis_top_monitoring_pt3d_mm,\
							m_pmarkers->right_bsis_mid_monitoring_pt3d_mm,\
							m_pmarkers->right_bsis_bot_monitoring_pt3d_mm,\
							m_pmarkers->left_bsis_top_warning_pt3d_mm,\
							m_pmarkers->left_bsis_bot_warning_pt3d_mm,\
							m_pmarkers->left_bsis_top_monitoring_pt3d_mm,\
							m_pmarkers->left_bsis_mid_monitoring_pt3d_mm,\
							m_pmarkers->left_bsis_bot_monitoring_pt3d_mm);

			get_rear_guide(	m_pmarkers->right_rear_warning_pt3d_mm, \
							m_pmarkers->left_rear_warning_pt3d_mm, \
							m_pmarkers->center_rear_warning_pt3d_mm);

			glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

			glm::mat4 mvp = m_ppm->opm * view_matrix;
			generate_and_update_MOBSMesh_3D();

			if (m_pvehicleSignal->m_trigger.gear != GEAR_REVERSE)
			{
				if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.mois_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.mois_speed_min */)
				{
					renderMOBS(MOIS_WARNING_3D, m_mois_warning_color, m_mois_warning_alpha, mvp);
					renderMOBS(MOIS_MONITORING_3D, m_mois_monitoring_color, m_mois_monitoring_alpha, mvp);
				}
				else noop;

				if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.bsis_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.bsis_speed_min */)
				{
				#if 1
					if(m_pvehicleSignal->m_trigger.turn_signal != TURN_SIGNAL_LEFT)
				#else    
					if (m_pvehicleSignal->m_wheel_angle <= 0.0f)
				#endif
					{
						// turn right
						renderMOBS(RIGHT_BSIS_TOP_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
						renderMOBS(RIGHT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);

						renderMOBS(RIGHT_BSIS_TOP_MONITORING_3D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, mvp);
						renderMOBS(RIGHT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						renderMOBS(RIGHT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
					}
					else
					{
						// turn left
						renderMOBS(LEFT_BSIS_TOP_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
						renderMOBS(LEFT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);

						renderMOBS(LEFT_BSIS_TOP_MONITORING_3D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, mvp);
						renderMOBS(LEFT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						renderMOBS(LEFT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
					}
				}
				else noop;

				// LCA Area
				// if (m_pvehicleSignal->m_vehicle_velocity > m_pxml->m_mobs_parameter.bsis_lca_speed_min)
				// {
				// 	if(m_pvehicleSignal->m_trigger.turn_signal != TURN_SIGNAL_LEFT)
				// 	{
				// 		renderMOBS(RIGHT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
				// 		renderMOBS(RIGHT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
				// 	}
				// 	else
				// 	{
				// 		renderMOBS(LEFT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
				// 		renderMOBS(LEFT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);						
				// 	}
				// }
				// else noop;

			}
			else // REVERSE GEAR
			{
				if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.reverse_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.reverse_speed_min */)
				{
					renderMOBS(CENTER_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp);
#if (0)
					if (0 < m_pvehicleSignal->m_wheel_angle) // reverse right
						renderMOBS(LEFT_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp);
					else // reverse left
						renderMOBS(RIGHT_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp); 
			
					if (0 < m_pvehicleSignal->m_wheel_angle)
					{
						// reverse right
						renderMOBS(RIGHT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
						renderMOBS(RIGHT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
					}
					else
					{
						// reverse left
						renderMOBS(LEFT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
						renderMOBS(LEFT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
					}
#endif
				}
				else noop;
			}
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.mobs)
		{
			m_pxml->m_activation.mobs = false;
			runtime_error  err_msg = logger.svm_fatal("C2208101", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}
void sanMOBS::drawLayout1(glm::mat4& view_matrix)
{
	try
	{
		if (m_pxml->m_activation.mobs)
		{
			generate_trajectory(90.0f, m_max_distance);

			get_mois_guide(	m_pmarkers->mois_warning_pt3d_mm, \
							m_pmarkers->mois_monitoring_pt3d_mm);

			get_bsis_guide( m_pmarkers->right_bsis_top_warning_pt3d_mm, \
							m_pmarkers->right_bsis_bot_warning_pt3d_mm, \
							m_pmarkers->right_bsis_top_monitoring_pt3d_mm, \
							m_pmarkers->right_bsis_mid_monitoring_pt3d_mm, \
							m_pmarkers->right_bsis_bot_monitoring_pt3d_mm, \
							m_pmarkers->left_bsis_top_warning_pt3d_mm, \
							m_pmarkers->left_bsis_bot_warning_pt3d_mm, \
							m_pmarkers->left_bsis_top_monitoring_pt3d_mm, \
							m_pmarkers->left_bsis_mid_monitoring_pt3d_mm, \
							m_pmarkers->left_bsis_bot_monitoring_pt3d_mm);

			get_rear_guide(	m_pmarkers->right_rear_warning_pt3d_mm, \
							m_pmarkers->left_rear_warning_pt3d_mm, \
							m_pmarkers->center_rear_warning_pt3d_mm);

			switch (m_pxml->m_layout[1].view_mode)
			{
			case CAMVIEW3D_FRONT:
			case CAMVIEW3D_RIGHT:
			case CAMVIEW3D_REAR:
			case CAMVIEW3D_LEFT:
			{
				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);
				
				glm::mat4 mvp = m_ppm->ppm * view_matrix;
				generate_and_update_MOBSMesh_3D();

				if (m_pvehicleSignal->m_trigger.gear != GEAR_REVERSE)
				{
					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.mois_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.mois_speed_min */)
					{
						renderMOBS(MOIS_WARNING_3D, m_mois_warning_color, m_mois_warning_alpha, mvp);
						renderMOBS(MOIS_MONITORING_3D, m_mois_monitoring_color, m_mois_monitoring_alpha, mvp);
					}
					else noop;

					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.bsis_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.bsis_speed_min */)
					{
#if 1
						if(m_pvehicleSignal->m_trigger.turn_signal != TURN_SIGNAL_LEFT)
#else	 
						if (m_pvehicleSignal->m_wheel_angle <= 0.0f)
#endif
						{
							// turn right
							renderMOBS(RIGHT_BSIS_TOP_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
							renderMOBS(RIGHT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);

							renderMOBS(RIGHT_BSIS_TOP_MONITORING_3D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, mvp);
							renderMOBS(RIGHT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
							renderMOBS(RIGHT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						}
						else
						{
							// turn left
							renderMOBS(LEFT_BSIS_TOP_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
							renderMOBS(LEFT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);

							renderMOBS(LEFT_BSIS_TOP_MONITORING_3D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, mvp);
							renderMOBS(LEFT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
							renderMOBS(LEFT_BSIS_BOT_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						}
					}
					else noop;
				}
				else // REVERSE GEAR
				{
					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.reverse_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.reverse_speed_min */)
					{
						renderMOBS(CENTER_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp);
#if (0)
						if (0 < m_pvehicleSignal->m_wheel_angle) // reverse right
							renderMOBS(LEFT_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp);
						else // reverse left							
							renderMOBS(RIGHT_REAR_WARNING_3D, m_rear_warning_color, m_rear_warning_alpha, mvp);

						if (0 < m_pvehicleSignal->m_wheel_angle)
						{
							// reverse right
							renderMOBS(RIGHT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
							renderMOBS(RIGHT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						}
						else
						{
							// reverse left
							renderMOBS(LEFT_BSIS_BOT_WARNING_3D, m_bsis_warning_color, m_bsis_warning_alpha, mvp);
							renderMOBS(LEFT_BSIS_MID_MONITORING_3D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, mvp);
						}
#endif
					}
					else noop;
				}
				break;
			}
			case CAMVIEW2D_FRONT:
			case CAMVIEW2D_RIGHT:
			case CAMVIEW2D_REAR:
			case CAMVIEW2D_LEFT:
			{
				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

				GLint camID = m_pxml->m_layout[1].view_mode - CAMVIEW2D_FRONT;
				generate_and_update_MOBSMesh_2D(camID);

				if (m_pvehicleSignal->m_trigger.gear != GEAR_REVERSE)
				{
					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.mois_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.mois_speed_min */)
					{
						renderMOBS(MOIS_WARNING_2D, m_mois_warning_color, m_mois_warning_alpha, glm::mat4(1.0f));
						renderMOBS(MOIS_MONITORING_2D, m_mois_monitoring_color, m_mois_monitoring_alpha, glm::mat4(1.0f));
					}
					else noop;

					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.bsis_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.bsis_speed_min */)
					{
#if 1
						if(m_pvehicleSignal->m_trigger.turn_signal != TURN_SIGNAL_LEFT)
#else	 
						if (m_pvehicleSignal->m_wheel_angle <= 0.0f)
#endif
						{
							// turn right
							renderMOBS(RIGHT_BSIS_TOP_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_BOT_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));

							renderMOBS(RIGHT_BSIS_TOP_MONITORING_2D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
						else
						{
							// turn left
							renderMOBS(LEFT_BSIS_TOP_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));

							renderMOBS(LEFT_BSIS_TOP_MONITORING_2D, m_bsis_top_monitoring_color, m_bsis_top_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
					}
					else noop;
					
					// LCA
					if (m_pvehicleSignal->m_vehicle_velocity > m_pxml->m_mobs_parameter.bsis_lca_speed_min)
					{
						if(m_pvehicleSignal->m_vehicle_velocity > m_pxml->m_mobs_parameter.bsis_lca_speed_min)
						{
							renderMOBS(RIGHT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
						else
						{
							renderMOBS(LEFT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
					}
					else noop;

				}
				else // REVERSE GEAR
				{
					if (m_pvehicleSignal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.reverse_speed_max /* && m_pvehicleSignal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.reverse_speed_min */)
					{
						renderMOBS(CENTER_REAR_WARNING_2D, m_rear_warning_color, m_rear_warning_alpha, glm::mat4(1.0f));
#if (0)
						if (0 < m_pvehicleSignal->m_wheel_angle) // reverse right
							renderMOBS(LEFT_REAR_WARNING_2D, m_rear_warning_color, m_rear_warning_alpha, glm::mat4(1.0f));
						else // reverse left
							renderMOBS(RIGHT_REAR_WARNING_2D, m_rear_warning_color, m_rear_warning_alpha, glm::mat4(1.0f));

						if (0 < m_pvehicleSignal->m_wheel_angle)
						{
							// reverse right
							renderMOBS(RIGHT_BSIS_BOT_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(RIGHT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
						else
						{
							// reverse left
							renderMOBS(RIGHT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_WARNING_2D, m_bsis_warning_color, m_bsis_warning_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_MID_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
							renderMOBS(LEFT_BSIS_BOT_MONITORING_2D, m_bsis_bot_monitoring_color, m_bsis_bot_monitoring_alpha, glm::mat4(1.0f));
						}
#endif
					}
					else noop;
				}
				break;
			}

			case CAMVIEW2D_ADD0:
				noop;
				break;

			default:
				throw runtime_error(string("$view_mode wrong"));
				break;
			}
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.mobs)
		{
			m_pxml->m_activation.mobs = false;
			runtime_error  err_msg = logger.svm_fatal("C2208201", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

void sanMOBS::generate_and_update_MOBSMesh_3D()
{
	try
	{
		//-------------------------------------MOIS warning------------------------------------------------
		int total_vertex_num = 0;
		GLfloat* mois_warning_lut = generate_3D_LUT(m_pmarkers->mois_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(mois_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(MOIS_WARNING_3D, mois_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (mois_warning_lut != nullptr) free(mois_warning_lut); else noop;

		//-------------------------------------MOIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* mois_monitoring_lut = generate_3D_LUT(m_pmarkers->mois_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(mois_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(MOIS_MONITORING_3D, mois_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (mois_monitoring_lut != nullptr) free(mois_monitoring_lut); else noop;

		//-------------------------------------RIGHT BSIS warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_bsis_top_warning_lut = generate_3D_LUT(m_pmarkers->right_bsis_top_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_bsis_top_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_TOP_WARNING_3D, right_bsis_top_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_top_warning_lut != nullptr) free(right_bsis_top_warning_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_bot_warning_lut = generate_3D_LUT(m_pmarkers->right_bsis_bot_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_bsis_bot_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_BOT_WARNING_3D, right_bsis_bot_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_bot_warning_lut != nullptr) free(right_bsis_bot_warning_lut); else noop;

		//-------------------------------------RIGHT BSIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_bsis_top_monitoring_lut = generate_3D_LUT(m_pmarkers->right_bsis_top_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_bsis_top_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_TOP_MONITORING_3D, right_bsis_top_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_top_monitoring_lut != nullptr) free(right_bsis_top_monitoring_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_mid_monitoring_lut = generate_3D_LUT(m_pmarkers->right_bsis_mid_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_bsis_mid_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_MID_MONITORING_3D, right_bsis_mid_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_mid_monitoring_lut != nullptr) free(right_bsis_mid_monitoring_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_bot_monitoring_lut = generate_3D_LUT(m_pmarkers->right_bsis_bot_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_bsis_bot_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_BOT_MONITORING_3D, right_bsis_bot_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_bot_monitoring_lut != nullptr) free(right_bsis_bot_monitoring_lut); else noop;

		//-------------------------------------LEFT BSIS warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_bsis_top_warning_lut = generate_3D_LUT(m_pmarkers->left_bsis_top_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_bsis_top_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_TOP_WARNING_3D, left_bsis_top_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_top_warning_lut != nullptr) free(left_bsis_top_warning_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_bot_warning_lut = generate_3D_LUT(m_pmarkers->left_bsis_bot_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_bsis_bot_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_BOT_WARNING_3D, left_bsis_bot_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_bot_warning_lut != nullptr) free(left_bsis_bot_warning_lut); else noop;

		//-------------------------------------LEFT BSIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_bsis_top_monitoring_lut = generate_3D_LUT(m_pmarkers->left_bsis_top_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_bsis_top_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_TOP_MONITORING_3D, left_bsis_top_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_top_monitoring_lut != nullptr) free(left_bsis_top_monitoring_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_mid_monitoring_lut = generate_3D_LUT(m_pmarkers->left_bsis_mid_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_bsis_mid_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_MID_MONITORING_3D, left_bsis_mid_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_mid_monitoring_lut != nullptr) free(left_bsis_mid_monitoring_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_bot_monitoring_lut = generate_3D_LUT(m_pmarkers->left_bsis_bot_monitoring_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_bsis_bot_monitoring_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_BOT_MONITORING_3D, left_bsis_bot_monitoring_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_bot_monitoring_lut != nullptr) free(left_bsis_bot_monitoring_lut); else noop;

		//-------------------------------------RIGHT REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_rear_warning_lut = generate_3D_LUT(m_pmarkers->right_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(right_rear_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_REAR_WARNING_3D, right_rear_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_rear_warning_lut != nullptr) free(right_rear_warning_lut); else noop;

		//-------------------------------------LEFT REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_rear_warning_lut = generate_3D_LUT(m_pmarkers->left_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(left_rear_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_REAR_WARNING_3D, left_rear_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_rear_warning_lut != nullptr) free(left_rear_warning_lut); else noop;

		//-------------------------------------CENTER REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* center_rear_warning_lut = generate_3D_LUT(m_pmarkers->center_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(center_rear_warning_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(CENTER_REAR_WARNING_3D, center_rear_warning_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (center_rear_warning_lut != nullptr) free(center_rear_warning_lut); else noop;

		is_3D_VAB_generated = true;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanMOBS::generate_and_update_MOBSMesh_2D(int camID)
{
	try
	{
		int total_vertex_num = 0;

		//-------------------------------------MOIS warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* mois_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->mois_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(mois_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(MOIS_WARNING_2D, mois_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (mois_warning_2d_lut != nullptr) free(mois_warning_2d_lut); else noop;

		//-------------------------------------MOIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* mois_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->mois_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(mois_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(MOIS_MONITORING_2D, mois_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (mois_monitoring_2d_lut != nullptr) free(mois_monitoring_2d_lut); else noop;

		//-------------------------------------RIGHT BSIS warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_bsis_top_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_bsis_top_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_bsis_top_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_TOP_WARNING_2D, right_bsis_top_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_top_warning_2d_lut != nullptr) free(right_bsis_top_warning_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_bot_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_bsis_bot_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_bsis_bot_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_BOT_WARNING_2D, right_bsis_bot_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_bot_warning_2d_lut != nullptr) free(right_bsis_bot_warning_2d_lut); else noop;

		//-------------------------------------RIGHT BSIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_bsis_top_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_bsis_top_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_bsis_top_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_TOP_MONITORING_2D, right_bsis_top_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_top_monitoring_2d_lut != nullptr) free(right_bsis_top_monitoring_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_mid_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_bsis_mid_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_bsis_mid_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_MID_MONITORING_2D, right_bsis_mid_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_mid_monitoring_2d_lut != nullptr) free(right_bsis_mid_monitoring_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* right_bsis_bot_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_bsis_bot_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_bsis_bot_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_BSIS_BOT_MONITORING_2D, right_bsis_bot_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_bsis_bot_monitoring_2d_lut != nullptr) free(right_bsis_bot_monitoring_2d_lut); else noop;

		//-------------------------------------LEFT BSIS warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_bsis_top_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_bsis_top_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_bsis_top_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_TOP_WARNING_2D, left_bsis_top_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_top_warning_2d_lut != nullptr) free(left_bsis_top_warning_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_bot_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_bsis_bot_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_bsis_bot_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_BOT_WARNING_2D, left_bsis_bot_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_bot_warning_2d_lut != nullptr) free(left_bsis_bot_warning_2d_lut); else noop;

		//-------------------------------------LEFT BSIS monitoring------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_bsis_top_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_bsis_top_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_bsis_top_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_TOP_MONITORING_2D, left_bsis_top_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_top_monitoring_2d_lut != nullptr) free(left_bsis_top_monitoring_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_mid_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_bsis_mid_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_bsis_mid_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_MID_MONITORING_2D, left_bsis_mid_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_mid_monitoring_2d_lut != nullptr) free(left_bsis_mid_monitoring_2d_lut); else noop;

		total_vertex_num = 0;
		GLfloat* left_bsis_bot_monitoring_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_bsis_bot_monitoring_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_bsis_bot_monitoring_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_BSIS_BOT_MONITORING_2D, left_bsis_bot_monitoring_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_bsis_bot_monitoring_2d_lut != nullptr) free(left_bsis_bot_monitoring_2d_lut); else noop;

		//-------------------------------------RIGHT REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* right_rear_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->right_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(right_rear_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(RIGHT_REAR_WARNING_2D, right_rear_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (right_rear_warning_2d_lut != nullptr) free(right_rear_warning_2d_lut); else noop;

		//-------------------------------------LEFT REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* left_rear_warning_2d_lut = generate_2D_LUT(camID, m_pmarkers->left_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(left_rear_warning_2d_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(LEFT_REAR_WARNING_2D, left_rear_warning_2d_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (left_rear_warning_2d_lut != nullptr) free(left_rear_warning_2d_lut); else noop;

		//-------------------------------------CENTER REAR warning------------------------------------------------
		total_vertex_num = 0;
		GLfloat* center_rear_warning_2D_lut = generate_2D_LUT(camID, m_pmarkers->center_rear_warning_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(center_rear_warning_2D_lut, total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(CENTER_REAR_WARNING_2D, center_rear_warning_2D_lut, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (center_rear_warning_2D_lut != nullptr) free(center_rear_warning_2D_lut); else noop;
		
		is_2D_VAB_generated = true;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

#define REMAP_INDEX(i,N) ( ((i)<(N)) ? 2*(i) : 4*(N)-2*(i)-1 )

GLfloat* sanMOBS::generate_3D_LUT(vector<Point3f> borders_pt3d_mm, int& total_vertex_num /*[out]*/)
{
	vector<Point3f> pt3d = sanWorld::get_Logic_InGLOBAL(m_pxml, borders_pt3d_mm);
	GLfloat* vtx = (GLfloat*)calloc(pt3d.size() * 3, sizeof(GLfloat));
	if (vtx != nullptr)
	{
		total_vertex_num = (int)pt3d.size();
		int N = total_vertex_num / 2;

		for (int i = 0; i < total_vertex_num; i++)
		{
			vtx[3 * REMAP_INDEX(i, N) + 0] = pt3d[i].x;
			vtx[3 * REMAP_INDEX(i, N) + 1] = pt3d[i].y;
			vtx[3 * REMAP_INDEX(i, N) + 2] = pt3d[i].z;
		}

		if (!pt3d.empty()) pt3d.clear(); else noop;
	}
	else
		throw runtime_error(__FUNCTION__ + delimiter(string("$failed to allocate memory")));

	return vtx;

}

vector<Point3f> sanMOBS::reorganizeBorders(int camID, vector<Point3f> borders_pt3d_mm)
{
	vector<Point3f> reorganizedBorders;

	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	int view_mode = camID + CAMVIEW2D_FRONT;
	const float shifting_mm = m_pxml->m_dgs_parameter.unit_square_length / 5.0f;

	switch (view_mode)
	{
	case CAMVIEW2D_FRONT:
	{
		for (int i = 0; i < (int)borders_pt3d_mm.size(); i++)
		{
			Point3f pt3d = borders_pt3d_mm[i];
			if (borders_pt3d_mm[i].y < vehicle_pt3d_mm[2].y)
				pt3d.y = vehicle_pt3d_mm[2].y - shifting_mm;
			else noop;
			reorganizedBorders.push_back(pt3d);
		}
		break;
	}
	case CAMVIEW2D_RIGHT:
	{
		for (int i = 0; i < (int)borders_pt3d_mm.size(); i++)
		{
			Point3f pt3d = borders_pt3d_mm[i];
			if (borders_pt3d_mm[i].x < vehicle_pt3d_mm[2].x)
				pt3d.x = vehicle_pt3d_mm[2].x - shifting_mm;
			else noop;
			reorganizedBorders.push_back(pt3d);
		}
		break;
	}
	case CAMVIEW2D_REAR:
	{
		for (int i = 0; i < (int)borders_pt3d_mm.size(); i++)
		{
			Point3f pt3d = borders_pt3d_mm[i];
			if (borders_pt3d_mm[i].y > vehicle_pt3d_mm[1].y)
				pt3d.y = vehicle_pt3d_mm[1].y + shifting_mm;
			else noop;
			reorganizedBorders.push_back(pt3d);
		}
		break;
	}
	case CAMVIEW2D_LEFT:
	{
		for (int i = 0; i < (int)borders_pt3d_mm.size(); i++)
		{
			Point3f pt3d = borders_pt3d_mm[i];
			if (borders_pt3d_mm[i].x > vehicle_pt3d_mm[1].x)
				pt3d.x = vehicle_pt3d_mm[1].x + shifting_mm;
			else noop;
			reorganizedBorders.push_back(pt3d);
		}
		break;
	}
	default:
		throw runtime_error(__FUNCTION__ + delimiter(string("$view_mode wrong")));
		break;
	}

	return reorganizedBorders;
}

// GLfloat* sanMOBS::generate_2D_LUT(int camID, vector<Point3f> borders_pt3d_mm, int& total_vertex_num /*out*/)
// {
// 	try
// 	{
// 		float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

// 		vector<Point3f> pt3d_mm = reorganizeBorders(camID, borders_pt3d_mm);
// 		vector<Point2f> pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, pt3d_mm);
// 		vector<Point2f> viewport_pt2d = sanWorld::normalize_points_in_defisheye(camID, m_pxml, pt2d);

// 		GLfloat* vtx = (GLfloat*)calloc(viewport_pt2d.size() * 3, sizeof(GLfloat));
// 		if (vtx != nullptr)
// 		{
// 			total_vertex_num = (int)viewport_pt2d.size();
// 			int N = total_vertex_num / 2;

// 			for (int i = 0; i < total_vertex_num; i++)
// 			{
// 				vtx[3 * REMAP_INDEX(i, N) + 0] = xflip * viewport_pt2d[i].x;
// 				vtx[3 * REMAP_INDEX(i, N) + 1] = -viewport_pt2d[i].y;
// 				vtx[3 * REMAP_INDEX(i, N) + 2] = 0.0f;
// 			}

// 			if (!viewport_pt2d.empty()) viewport_pt2d.clear(); else noop;
// 			if (!pt2d.empty()) pt2d.clear(); else noop;
// 			if (!pt3d_mm.empty()) pt3d_mm.clear(); else noop;
// 		}
// 		else
// 			throw runtime_error(string("$failed to allocate memory"));

// 		return vtx;
// 	}
// 	catch (exception& e)
// 	{
// 		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
// 	}	
// }


GLfloat* sanMOBS::generate_2D_LUT(int camID, vector<Point3f> borders_pt3d_mm, int& total_vertex_num /*out*/)
{
	try
	{
		float vertex_xnorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].cols - (m_pxml->m_rcam[camID].camview_offset.hleft + m_pxml->m_rcam[camID].camview_offset.hright));
		float vertex_ynorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].rows - (m_pxml->m_rcam[camID].camview_offset.vtop + m_pxml->m_rcam[camID].camview_offset.vbot));

		float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

		vector<Point3f> pt3d_mm = reorganizeBorders(camID, borders_pt3d_mm);
		vector<Point2f> pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, pt3d_mm);

		GLfloat* vtx = (GLfloat*)calloc(pt2d.size() * 3, sizeof(GLfloat));
		if (vtx != nullptr)
		{
			total_vertex_num = (int)pt2d.size();
			int N = total_vertex_num / 2;

			for (int i = 0; i < total_vertex_num; i++)
			{
				vtx[3 * REMAP_INDEX(i, N) + 0] = xflip * ((pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
				vtx[3 * REMAP_INDEX(i, N) + 1] = -((pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
				vtx[3 * REMAP_INDEX(i, N) + 2] = 0.0f;
			}

			if (!pt2d.empty()) pt2d.clear(); else noop;
			if (!pt3d_mm.empty()) pt3d_mm.clear(); else noop;
		}
		else
			throw runtime_error(string("$failed to allocate memory"));

		return vtx;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}	
}


void sanMOBS::get_mois_guide(	vector<Point3f>& mois_warning_pt3d_mm /*[out]*/,\
								vector<Point3f>& mois_monitoring_pt3d_mm /*[out]*/)
{
	if (!mois_warning_pt3d_mm.empty()) mois_warning_pt3d_mm.clear(); else noop;
	if (!mois_monitoring_pt3d_mm.empty()) mois_monitoring_pt3d_mm.clear(); else noop;

	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	/* vertex order of the vehicle box, based on GL
		v0 ------ v2		v3 ------ v2		v2 ------ v1
		|	      |			| markers |			| markers |
		| vehicle |			|         |			|         |
		|	      |			|  filled |			|   loop  |
		v1 ------ v3		v1 ------ v0		v3 ------ v0 */

	float L = (m_pxml->m_vehicle_spec.wheel_base / 2.0f) + m_pxml->m_vehicle_spec.front_overhang;

	//--------------------------------------------------------------------
	// mois warning
	//--------------------------------------------------------------------
	int Sw = 1 + (int)(m_pxml->m_mobs_parameter.mois_dist_warning_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float Ww = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.mois_dist_warning_side;
	
	for (int i = 0; i < Sw; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// right
		mois_warning_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L + s * Ww, s * L - c * Ww, 0.0f));
	}
	for (int i = Sw; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i-1]);
		float c = cos(m_vehicle_heading_rad[i-1]);
		// left
		mois_warning_pt3d_mm.push_back(m_center_pt3d_mm[i-1] + Point3f(c * L - s * Ww, s * L + c * Ww, 0.0f));
	}

	//--------------------------------------------------------------------
	// mois mornitoring
	//--------------------------------------------------------------------
	int Sm = 1 + (int)(m_pxml->m_mobs_parameter.mois_dist_monitoring_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float Wm = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.mois_dist_monitoring_side;

	for (int i = 0; i < Sm; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// right
		mois_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L + s * Wm, s * L - c * Wm, 0.0f));
	}

	for (int i = Sm; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i - 1]);
		float c = cos(m_vehicle_heading_rad[i - 1]);
		// left
		mois_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i - 1] + Point3f(c * L - s * Wm, s * L + c * Wm, 0.0f));
	}
}

void sanMOBS::get_bsis_guide(	vector<Point3f>& right_bsis_top_warning_pt3d_mm /*[out]*/,\
								vector<Point3f>& right_bsis_bot_warning_pt3d_mm /*[out]*/, \
								vector<Point3f>& right_bsis_top_monitoring_pt3d_mm /*[out]*/,\
								vector<Point3f>& right_bsis_mid_monitoring_pt3d_mm /*[out]*/, \
								vector<Point3f>& right_bsis_bot_monitoring_pt3d_mm /*[out]*/,\
								vector<Point3f>& left_bsis_top_warning_pt3d_mm /*[out]*/, \
								vector<Point3f>& left_bsis_bot_warning_pt3d_mm /*[out]*/, \
								vector<Point3f>& left_bsis_top_monitoring_pt3d_mm /*[out]*/, \
								vector<Point3f>& left_bsis_mid_monitoring_pt3d_mm /*[out]*/, \
								vector<Point3f>& left_bsis_bot_monitoring_pt3d_mm /*[out]*/)
{
	if (!right_bsis_top_warning_pt3d_mm.empty()) right_bsis_top_warning_pt3d_mm.clear(); else noop;
	if (!right_bsis_bot_warning_pt3d_mm.empty()) right_bsis_bot_warning_pt3d_mm.clear(); else noop;
	if (!right_bsis_top_monitoring_pt3d_mm.empty()) right_bsis_top_monitoring_pt3d_mm.clear(); else noop;
	if (!right_bsis_mid_monitoring_pt3d_mm.empty()) right_bsis_mid_monitoring_pt3d_mm.clear(); else noop;
	if (!right_bsis_bot_monitoring_pt3d_mm.empty()) right_bsis_bot_monitoring_pt3d_mm.clear(); else noop;
	if (!left_bsis_top_warning_pt3d_mm.empty()) left_bsis_top_warning_pt3d_mm.clear(); else noop;
	if (!left_bsis_bot_warning_pt3d_mm.empty()) left_bsis_bot_warning_pt3d_mm.clear(); else noop;
	if (!left_bsis_top_monitoring_pt3d_mm.empty()) left_bsis_top_monitoring_pt3d_mm.clear(); else noop;
	if (!left_bsis_mid_monitoring_pt3d_mm.empty()) left_bsis_mid_monitoring_pt3d_mm.clear(); else noop;
	if (!left_bsis_bot_monitoring_pt3d_mm.empty()) left_bsis_bot_monitoring_pt3d_mm.clear(); else noop;

	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	/* vertex order of the vehicle box, based on GL
		v0 ------ v2		v3 ------ v2		v2 ------ v1
		|	      |			| markers |			| markers |
		| vehicle |			|         |			|         |
		|	      |			|  filled |			|   loop  |
		v1 ------ v3		v1 ------ v0		v3 ------ v0 */
	
	float L = (m_pxml->m_vehicle_spec.wheel_base / 2.0f) + m_pxml->m_vehicle_spec.front_overhang;
	float vehicle_length = vehicle_pt3d_mm[2].y - vehicle_pt3d_mm[3].y;
	//--------------------------------------------------------------------
	// right bsis top warning
	//--------------------------------------------------------------------
	int RSwf = 1 + (int)(m_pxml->m_mobs_parameter.right_bsis_dist_warning_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float RWwfs = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.right_bsis_dist_warning_side;
	float RWwf = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f;

	for (int i = 0; i < RSwf; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// outer right
		right_bsis_top_warning_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L + s * RWwfs, s * L - c * RWwfs, 0.0f));
	}
	for (int i = RSwf; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i - 1]);
		float c = cos(m_vehicle_heading_rad[i - 1]);
		// inner right
		right_bsis_top_warning_pt3d_mm.push_back(m_center_pt3d_mm[i - 1] + Point3f(c * L + s * RWwf, s * L - c * RWwf, 0.0f));
	}

	//--------------------------------------------------------------------
	// right bsis bot warning
	//--------------------------------------------------------------------
	right_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[2] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_warning_side, -(m_pxml->m_mobs_parameter.right_bsis_dist_warning_tail), 0.0f));
	right_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[2] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_warning_side, 0.0f, 0.0f));
	right_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[2] + Point3f(0.0f, 0.0f, 0.0f));
	right_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[2] + Point3f(0.0f, -(m_pxml->m_mobs_parameter.right_bsis_dist_warning_tail), 0.0f));
	//--------------------------------------------------------------------
	// right bsis top mornitoring
	//--------------------------------------------------------------------
	int RSmf = 1 + (int)(m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float RWmfs = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_side;
	float RWmf = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f;

	for (int i = 0; i < RSmf; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// outer right
		right_bsis_top_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L + s * RWmfs, s * L - c * RWmfs, 0.0f));
	}
	for (int i = RSmf; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i - 1]);
		float c = cos(m_vehicle_heading_rad[i - 1]);
		// inner right
		right_bsis_top_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i - 1] + Point3f(c * L + s * RWmf, s * L - c * RWmf, 0.0f));
	}

	//--------------------------------------------------------------------
	// right bsis middle mornitoring
	//--------------------------------------------------------------------
	right_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_side, 0.0f, 0.0f));
	right_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[2] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_side, 0.0f, 0.0f));
	right_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[2]);
	right_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3]);

	//--------------------------------------------------------------------
	// right bsis bottom mornitoring
	//--------------------------------------------------------------------
	right_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_side, vehicle_length - m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_tail, 0.0f));
	right_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_side, 0.0f, 0.0f));
	right_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(0.0f, 0.0f, 0.0f));
	right_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(0.0f, vehicle_length - m_pxml->m_mobs_parameter.right_bsis_dist_monitoring_tail, 0.0f));

	//--------------------------------------------------------------------
	// left bsis top warning
	//--------------------------------------------------------------------
	int LSwf = 1 + (int)(m_pxml->m_mobs_parameter.left_bsis_dist_warning_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float LWwfs = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.left_bsis_dist_warning_side;
	float LWwf = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f;
	
	for (int i = 0; i < LSwf; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// inner left
		left_bsis_top_warning_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L - s * LWwf, s * L + c * LWwf, 0.0f));
	}
	for (int i = LSwf; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i - 1]);
		float c = cos(m_vehicle_heading_rad[i - 1]);
		// outer left
		left_bsis_top_warning_pt3d_mm.push_back(m_center_pt3d_mm[i - 1] + Point3f(c * L - s * LWwfs, s * L + c * LWwfs, 0.0f));
	}
	
	//--------------------------------------------------------------------
	// left bsis bot warning
	//--------------------------------------------------------------------
	left_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[0] + Point3f(0.0f, -(m_pxml->m_mobs_parameter.left_bsis_dist_warning_tail), 0.0f));
	left_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[0] + Point3f(0.0f, 0.0f, 0.0f));
	left_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[0] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_warning_side, 0.0f, 0.0f));
	left_bsis_bot_warning_pt3d_mm.push_back(vehicle_pt3d_mm[0] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_warning_side, -(m_pxml->m_mobs_parameter.left_bsis_dist_warning_tail), 0.0f));

	//--------------------------------------------------------------------
	// left bsis top mornitoring
	//--------------------------------------------------------------------
	int LSmf = 1 + (int)(m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_head / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float LWmfs = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f + m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_side;
	float LWmf = (vehicle_pt3d_mm[2].x - vehicle_pt3d_mm[0].x) / 2.0f;

	for (int i = 0; i < LSmf; i++)
	{
		float s = sin(m_vehicle_heading_rad[i]);
		float c = cos(m_vehicle_heading_rad[i]);
		// inner left
		left_bsis_top_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i] + Point3f(c * L - s * LWmf, s * L + c * LWmf, 0.0f));
	}
	for (int i = LSmf; i > 0; i--)
	{
		float s = sin(m_vehicle_heading_rad[i - 1]);
		float c = cos(m_vehicle_heading_rad[i - 1]);
		// outer left
		left_bsis_top_monitoring_pt3d_mm.push_back(m_center_pt3d_mm[i - 1] + Point3f(c * L - s * LWmfs, s * L + c * LWmfs, 0.0f));
	}

	//--------------------------------------------------------------------
	// left bsis middle mornitoring
	//--------------------------------------------------------------------
	left_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1]); 
	left_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[0]);
	left_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[0] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_side, 0.0f, 0.0f));
	left_bsis_mid_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_side, 0.0f, 0.0f));

	//--------------------------------------------------------------------
	// left bsis bottom mornitoring
	//--------------------------------------------------------------------
	left_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(0.0f, vehicle_length - m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_tail, 0.0f));
	left_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(0.0f, 0.0f, 0.0f));
	left_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_side, 0.0f, 0.0f));
	left_bsis_bot_monitoring_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_side, vehicle_length - m_pxml->m_mobs_parameter.left_bsis_dist_monitoring_tail, 0.0f));

}

void sanMOBS::get_rear_guide(	vector<Point3f>& right_rear_warning_pt3d_mm /*[out]*/, \
								vector<Point3f>& left_rear_warning_pt3d_mm /*[out]*/, \
								vector<Point3f>& center_rear_warning_pt3d_mm /*[out]*/)
{
	if (!right_rear_warning_pt3d_mm.empty()) right_rear_warning_pt3d_mm.clear(); else noop;
	if (!left_rear_warning_pt3d_mm.empty()) left_rear_warning_pt3d_mm.clear(); else noop;
	if (!center_rear_warning_pt3d_mm.empty()) center_rear_warning_pt3d_mm.clear(); else noop;

	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	/* vertex order of the vehicle box, based on GL
		v0 ------ v2		v3 ------ v2		v2 ------ v1
		|	      |			| markers |			| markers |
		| vehicle |			|         |			|         |
		|	      |			|  filled |			|   loop  |
		v1 ------ v3		v1 ------ v0		v3 ------ v0 */

	//--------------------------------------------------------------------
	// right rear mornitoring
	//--------------------------------------------------------------------
	right_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.right_rear_dist_warning_side, -m_pxml->m_mobs_parameter.right_rear_dist_warning_tail, 0.0f));
	right_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.right_rear_dist_warning_side, 0.0f, 0.0f));
	right_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(0.0f, 0.0f, 0.0f));
	right_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(0.0f, -m_pxml->m_mobs_parameter.right_rear_dist_warning_tail, 0.0f));

	//--------------------------------------------------------------------
	// left rear warning
	//--------------------------------------------------------------------
	left_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(0.0f, -m_pxml->m_mobs_parameter.left_rear_dist_warning_tail, 0.0f));
	left_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(0.0f, 0.0f, 0.0f));
	left_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.left_rear_dist_warning_side, 0.0f, 0.0f));
	left_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.left_rear_dist_warning_side, -m_pxml->m_mobs_parameter.left_rear_dist_warning_tail, 0.0f));

	//--------------------------------------------------------------------
	// center rear warning
	//--------------------------------------------------------------------
	center_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.center_rear_dist_warning_side, -m_pxml->m_mobs_parameter.center_rear_dist_warning_far_tail, 0.0f));
	center_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[3] + Point3f(m_pxml->m_mobs_parameter.center_rear_dist_warning_side, -m_pxml->m_mobs_parameter.center_rear_dist_warning_near_tail, 0.0f));
	center_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.center_rear_dist_warning_side, -m_pxml->m_mobs_parameter.center_rear_dist_warning_near_tail, 0.0f));
	center_rear_warning_pt3d_mm.push_back(vehicle_pt3d_mm[1] + Point3f(-m_pxml->m_mobs_parameter.center_rear_dist_warning_side, -m_pxml->m_mobs_parameter.center_rear_dist_warning_far_tail, 0.0f));
}

void sanMOBS::generate_trajectory(float vehicle_initial_heading /* [in] degree */, float distance_travel_mm /* [in] mm */)
{
	if (!m_center_pt3d_mm.empty()) m_center_pt3d_mm.clear(); else noop;
	if (!m_vehicle_heading_rad.empty()) m_vehicle_heading_rad.clear(); else noop;

	Point3f center_pt3d_mm(0.0f, 0.0f, 0.0f);
	float vehicle_heading_rad = float(vehicle_initial_heading * M_PI / 180.0f);

	float vehicle_velocity = m_pvehicleSignal->m_vehicle_velocity;

	if (vehicle_velocity == 0)	vehicle_velocity = 5.0f;
	else noop;

	//vehicle_velocity *= (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE ? -1.0f : 1.0f);
	
	float vehicle_velocity_mps = vehicle_velocity * 1000.0f / 3600.0f;
	int sample_num = 1 + (int)(distance_travel_mm / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	float sample_time = abs(distance_travel_mm / (1000.0f * vehicle_velocity_mps * sample_num));

	float wheel_angle = m_pvehicleSignal->m_wheel_angle;
	float wheel_angle_rad = float(wheel_angle * M_PI / 180.0f);

	Point3f base_position = (wheel_angle >= 0.0f) ? m_ppgs->m_pgs_wheel_box_mm[1] : m_ppgs->m_pgs_wheel_box_mm[3];
	Point3f travel_position(0.0f);

	for (int i = 0; i < sample_num; i++)
	{
		if (i == 0)
		{
			travel_position = base_position + ((wheel_angle >= 0.0f) ? 1.0f : -1.0f) * Point3f(m_ppgs->m_rear_track_width_mm * sin(vehicle_heading_rad), -m_ppgs->m_rear_track_width_mm * cos(vehicle_heading_rad), 0.0f);
		}
		else
		{
			float dx = 1000.0f * vehicle_velocity_mps * cos(vehicle_heading_rad);	// mm
			float dy = 1000.0f * vehicle_velocity_mps * sin(vehicle_heading_rad);	// mm
			float dh = (1000.0f * vehicle_velocity_mps) / ((m_ppgs->m_rear_track_width_mm - m_ppgs->m_front_track_width_mm) / 2.0f + m_ppgs->m_wheel_base_mm / tan(wheel_angle_rad));	// radian

			travel_position.x += sample_time * dx;
			travel_position.y += sample_time * dy;
			travel_position.z += 0;
			vehicle_heading_rad += sample_time * dh;
		}

		if (wheel_angle >= 0.0f)	// Left wheel based, distance travelled calculated on right wheel
		{
			center_pt3d_mm = travel_position + Point3f(	cos(vehicle_heading_rad) * m_ppgs->m_wheel_base_mm / 2.0f - sin(vehicle_heading_rad) * m_ppgs->m_rear_track_width_mm / 2.0f,\
														sin(vehicle_heading_rad) * m_ppgs->m_wheel_base_mm / 2.0f + cos(vehicle_heading_rad) * m_ppgs->m_rear_track_width_mm / 2.0f,\
														0.0f);
		}
		else						// Right wheel based, distance travelled calculated on left wheel
		{
			center_pt3d_mm = travel_position + Point3f(	cos(vehicle_heading_rad) * m_ppgs->m_wheel_base_mm / 2.0f + sin(vehicle_heading_rad) * m_ppgs->m_rear_track_width_mm / 2.0f,\
														sin(vehicle_heading_rad) * m_ppgs->m_wheel_base_mm / 2.0f - cos(vehicle_heading_rad) * m_ppgs->m_rear_track_width_mm / 2.0f,\
														0.0f);
		}

		m_center_pt3d_mm.push_back(center_pt3d_mm);
		m_vehicle_heading_rad.push_back(vehicle_heading_rad);
	}
}
