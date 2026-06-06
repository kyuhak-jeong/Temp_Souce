#include <errno.h>
#include <memory.h>
#include "svmXML.hpp"
#include "svmLogger.hpp"
#include "svmWorld.hpp"
#include "svmFromFile.hpp"


static inline string pack_message(char* tag, char* src)  //for checking error and exception
{
	return "<" + string(tag) + ">" + string(src) + "</" + string(tag) + ">";
}

static void PRE_PROCESSING(stringstream& ss, char* src) //for checking error and exception
{
	char tmp_data[5000];
	memset(tmp_data, 0x00, sizeof(tmp_data));
	string(src).copy(tmp_data, strlen(src), 0);
	string new_src((char *)tmp_data);

	std::replace_if(new_src.begin(), new_src.end(), [=](char& c) {return (c == '\t' || c == '\n' || c=='\a');}, ' ');
	ss.str(new_src);

	//ss.exceptions(stringstream::failbit | stringstream::badbit | stringstream::hex | stringstream::hexfloat);
	ss.exceptions(stringstream::failbit | stringstream::badbit);
}

static void POST_PROCESSING(stringstream& ss, char* src, char* tag) //for checking error and exception
{
	if (!ss.eof())
	{
		string dummy("");
		std::getline(ss, dummy);
		dummy.erase(std::remove_if(dummy.begin(), dummy.end(), ::isspace), dummy.end());
		if (dummy.length())	throw runtime_error(pack_message(tag, src));
		else noop;
	}
}

sanXML::sanXML()
{
	this->m_settings_common_filePath = string(_SETTINGS_PATH_) + string("/settings_common.xml");
	this->m_settings_calibration_filePath = string(_SETTINGS_PATH_) + string("/settings_calibration.xml");
	this->m_output_calibration_filePath = string(_SETTINGS_PATH_) + string("/output_calibration.xml");
	this->m_settings_view_filePath = string(_SETTINGS_PATH_) + string("/settings_view.xml");
	this->m_output_calibration_result_image_filePath = string(_OUTPUTS_PATH_) + string("/calibration_result.jpg");

	memset(m_calibrated_parameters, 0x00, sizeof(m_calibrated_parameters));
	memset((void*)&m_calibrated_status, 0x00, sizeof(m_calibrated_status));
}

sanXML::~sanXML()
{
	if(!m_mask_seam.empty()) m_mask_seam.clear();
}

void sanXML::initialize()
{
	try
	{
		xmlLoad(m_settings_common_filePath);

		xmlDocPtr xml_doc_ptr = nullptr;
		xml_doc_ptr = xmlReadFile((const char*)&m_output_calibration_filePath.c_str()[0], nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
		if (xml_doc_ptr == nullptr)
		{
			create_output_calibration_file(m_output_calibration_filePath);
		}
		else
		{
			xmlFreeDoc(xml_doc_ptr);
			xml_doc_ptr = nullptr;
			xmlCleanupParser();
		}

#ifdef CALIBRATION_APP
		//create_output_calibration_file(m_output_calibration_filePath);
		xmlLoad(m_settings_calibration_filePath);
#endif // CALIBRATION_APP

#ifdef VIEWER_APP
		xmlLoad(m_output_calibration_filePath);

		if (m_calibrated_status.calibration_done != 1)
		{
			throw runtime_error(string("Please run the calibration app again before running the view app!"));
		}
		else noop;

		xmlLoad(m_settings_view_filePath);
		loadMaskSeam();
#endif // VIEWER_APP

		check_range_all_parameters();

#ifdef CALIBRATION_APP
		calPoster();
		calcLocalPoints();
#endif // CALIBRATION_APP
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("Cx101101", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanXML::calPoster()
{
	m_arrangement.poster.width = 4 * m_pattern.unit_space +
								m_arrangement.pattern_offset_from_car.left_pattern_offset +
								m_vehicle_spec.width +
								m_arrangement.pattern_offset_from_car.right_pattern_offset;

	m_arrangement.poster.height = 4 * m_pattern.unit_space +
								m_arrangement.pattern_offset_from_car.front_pattern_offset +
								m_vehicle_spec.length +
								m_arrangement.pattern_offset_from_car.rear_pattern_offset;

	m_arrangement.poster.radius_scale = m_grid.base_radius_times;
}

void sanXML::calcLocalPoints()
{
	try
	{
		memset(m_global_pts, 0x00, sizeof(XYZ) * PATTERN_MAX_NUM * PATTERN_POINTS_NUM);
		sanWorld::generate_real_pattern_point_mm_in_GLOBAL(this, m_global_pts);


		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";

			try
			{
				// memset(m_local_pts[camID], 0x00, sizeof(XYZ) * CONTROL_POINTS_NUM);
				std::fill(m_local_pts[camID], m_local_pts[camID] + CONTROL_POINTS_NUM, XYZ());
				sanWorld::get_real_pattern_point_mm_in_PATTERN(camID, this, m_local_pts[camID]);
			}
			catch (exception& e)
			{
				throw runtime_error(str_camID + delimiter(string(e.what())));
			}
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanXML::loadMaskSeam()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string full_file_path = string(_ARRAYS_PATH_) + "/seam" + to_string(camID + 1);
		m_mask_seam.push_back(sanFromFile::read_point_2d(full_file_path));
	}
	
}

void sanXML::xmlLoad(string filePath)
{
	try
	{
		xmlResetLastError();

		const char* filepath = (const char*)&filePath.c_str()[0];
		xmlDocPtr xml_doc_ptr = nullptr;
		xml_doc_ptr = xmlReadFile(filepath, nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
		if (xml_doc_ptr != nullptr)
		{
			xmlNodePtr root_node_ptr = xmlDocGetRootElement(xml_doc_ptr); //LEVEL1(Root)
			if (root_node_ptr != nullptr)
			{
				for (xmlNodePtr node_ptr = root_node_ptr->children; node_ptr != nullptr; node_ptr = node_ptr->next) //LEVEL2
				{
					if (node_ptr->type == XML_ELEMENT_NODE)
					{
						for (xmlNodePtr children_node_ptr = node_ptr->children; children_node_ptr != nullptr; children_node_ptr = children_node_ptr->next) //LEVEL3
						{
							if (children_node_ptr->type == XML_ELEMENT_NODE)
							{
								try
								{
									int code = getCodeFromName((char*)children_node_ptr->name);
									char* str_value = (char*)xmlNodeGetContent(children_node_ptr);
									if(str_value != nullptr)
									{
										convertStringToDatatype((char*)children_node_ptr->name, code, str_value);

										if (const xmlError* xml_error = xmlGetLastError())
										{
											xmlFreeDoc(xml_doc_ptr);
											xml_doc_ptr = nullptr;
											xmlCleanupParser();
											throw runtime_error(xml_error->message);
										}
										else noop;

										xmlFree(str_value);
									}
									else noop;
								}
								catch (exception& e)
								{
									xmlFreeDoc(xml_doc_ptr);
									xml_doc_ptr = nullptr;
									xmlCleanupParser();
									throw runtime_error(string(e.what())); // propagation to the parent catch-statement
								}
							} 
							else noop;
						}
					}
					else noop;
				}
			}
			else noop;

			xmlFreeDoc(xml_doc_ptr);
			xml_doc_ptr = nullptr;
			xmlCleanupParser();
		}
		else
		{
			const xmlError* xml_error = xmlGetLastError();
			if (xml_error != nullptr) throw runtime_error(string(xml_error->message));
			else noop;
		}
	} 
	catch (exception& e)
	{	
		throw runtime_error(__FUNCTION__+ delimiter(string(e.what())));
	}
}

int sanXML::getCodeFromName(const char* name)
{
	int code = 0;

	// system parameter
	if (strcmp(name, "display_resolution") == 0) code = 10100;
	else if (strcmp(name, "image_resolution") == 0) code = 10110;
	else if (strcmp(name, "cameras_number") == 0) code = 10120;
	else if (strcmp(name, "patterns_number") == 0) code = 10130;

	// calibration parameter
	else if (strcmp(name, "calibration_type") == 0) code = 10140;

	// real cameras(intrinsic)
	else if (strcmp(name, "rcam0") == 0) code = 10200;
	else if (strcmp(name, "rcam1") == 0) code = 10201;
	else if (strcmp(name, "rcam2") == 0) code = 10202;
	else if (strcmp(name, "rcam3") == 0) code = 10203;
	// real additional cameras(intrinsic)
	else if (strcmp(name, "acam0") == 0) code = 10250;

	// virtual cameras(pose)
	else if (strcmp(name, "vcam0") == 0) code = 10300;
	else if (strcmp(name, "vcam1") == 0) code = 10301;
	else if (strcmp(name, "vcam2") == 0) code = 10302;
	else if (strcmp(name, "vcam3") == 0) code = 10303;
	else if (strcmp(name, "vcam10") == 0) code = 10310;

	// projection
	else if (strcmp(name, "orthographic") == 0) code = 10400;
	else if (strcmp(name, "perspective") == 0) code = 10410;

	// layout and view
	else if (strcmp(name, "layout0") == 0) code = 10500;
	else if (strcmp(name, "layout1") == 0) code = 10501;
	// view setting
	else if (strcmp(name, "view_setting") == 0) code = 10510;

	// image processing
	else if (strcmp(name, "contour") == 0) code = 10600;

	// space grid
	else if (strcmp(name, "bowl3D") == 0) code = 10610;

	// camview
	
	// vehicle model
	else if (strcmp(name, "model_file") == 0) code = 10700;
	else if (strcmp(name, "model_translation") == 0) code = 10720;
	else if (strcmp(name, "model_scale") == 0) code = 10730;
	else if (strcmp(name, "model_rotation") == 0) code = 10740;

	// real vehicle
	else if (strcmp(name, "vehicle_information") == 0) code = 10800;
	else if (strcmp(name, "vehicle_specification") == 0) code = 10810;

	// function and parameters
	else if (strcmp(name, "activation") == 0) code = 10900;
	else if (strcmp(name, "scene_animation_parameter") == 0) code = 10910;
	else if (strcmp(name, "model_animation_parameter") == 0) code = 10920;
	else if (strcmp(name, "model_transparency_parameter") == 0) code = 10921;
	else if (strcmp(name, "model_lamp_parameter") == 0) code = 10922;
	else if (strcmp(name, "pgs_parameter") == 0) code = 10930;
	else if (strcmp(name, "dgs_parameter") == 0) code = 10940;
	else if (strcmp(name, "od_parameter") == 0) code = 10950;
	else if (strcmp(name, "mobs_parameter") == 0) code = 10960;
	else if (strcmp(name, "vbc_parameter") == 0) code = 10970;

	// pattern
	else if (strcmp(name, "unit_space") == 0) code = 11000;

	// arrangement
	else if (strcmp(name, "pattern_offset_from_car") == 0) code = 11100;
	else if (strcmp(name, "distance_between_patterns") == 0) code = 11110;
	else if (strcmp(name, "poster_size") == 0) code = 11120;

	// feature points
	else if (strcmp(name, "rcam0_feature") == 0) code = 11200;
	else if (strcmp(name, "rcam1_feature") == 0) code = 11201;
	else if (strcmp(name, "rcam2_feature") == 0) code = 11202;
	else if (strcmp(name, "rcam3_feature") == 0) code = 11203;

	// local points
	else if (strcmp(name, "rcam0_local") == 0) code = 11300;
	else if (strcmp(name, "rcam1_local") == 0) code = 11301;
	else if (strcmp(name, "rcam2_local") == 0) code = 11302;
	else if (strcmp(name, "rcam3_local") == 0) code = 11303;

	// calibrated parameters
	else if (strcmp(name, "rcam0_calibrated") == 0) code = 11400;
	else if (strcmp(name, "rcam1_calibrated") == 0) code = 11401;
	else if (strcmp(name, "rcam2_calibrated") == 0) code = 11402;
	else if (strcmp(name, "rcam3_calibrated") == 0) code = 11403;

	// calibrated status
	else if (strcmp(name, "calibrated_status") == 0) code = 11500;
	else code = 99999;

	return (code);
}

void sanXML::convertStringToDatatype(char* tag, int code, char* str_val)
{
	switch (code)
	{
		case 10100: read_xy(tag, str_val, &m_resolution.display); break;
		case 10110:	read_xy(tag, str_val, &m_resolution.image); 	break;
		case 10120:	read_int(tag, str_val, &m_svm_cameras_num); break;
		case 10130:	read_int(tag, str_val, &m_svm_patterns_num); break;
		case 10140:	read_int(tag, str_val, &m_calibration_type);	break;

		// real camera(intrinsic)
		case 10200: case 10201: case 10202: case 10203: read_rcam_parameters(tag, str_val, &m_rcam[code - 10200]); break;
		case 10250: read_rcam_parameters(tag, str_val, &m_acam[code - 10250]); break;
		// virtual camera(pose)
		case 10300: case 10301: case 10302: case 10303: case 10310:	read_vcam_pose(tag, str_val, &m_vcam[code - 10300]);	break;
		// projection
		case 10400:	read_projection_orthographic(tag, str_val, &m_orthogrphic);	break;
		case 10410:	read_projection_perspective(tag, str_val, &m_perspective);	break;
		// layout and view
		case 10500:	case 10501:	read_layout(tag, str_val, &m_layout[code - 10500]);	break;
		case 10510: read_view_setting(tag, str_val, &m_view_setting); break;
		// contour
		case 10600:	read_contour(tag, str_val, &m_contour);	break;
		// grid bowl
		case 10610:	read_bowl3d(tag, str_val, &m_grid);	break;
		// camview
		
		// vehicle model
		case 10700:	m_model.model_file = string(str_val); break;
		case 10720:	read_xyz(tag, str_val, &m_model.translation); break;
		case 10730:	read_xyz(tag, str_val, &m_model.scale); break;
		case 10740:	read_xyz(tag, str_val, &m_model.rotation); break;
		// real vehicle
		case 10800: read_vehicle_information(tag, str_val, &m_vehicle_info); break;
		case 10810: read_vehicle_specification(tag, str_val, &m_vehicle_spec); break;
		// function
		case 10900:	read_activation(tag, str_val, &m_activation); break;
		// scene animation parameter
		case 10910: read_scene_animation_parameter(tag, str_val, &m_scene_animation_parameter);	break;	
		// model animation parameter
		case 10920: read_model_animation_parameter(tag, str_val, &m_model_animation_parameter);	break;
		// model transparency parameter
		case 10921:	read_float(tag, str_val, &m_model_transparency_parameter.rate);	break;
		// model lamp parameter
		case 10922: read_model_lamp_parameter(tag, str_val, &m_model_lamp_parameter); break;
		// pgs parameter
		case 10930: read_pgs_parameter(tag, str_val, &m_pgs_parameter); 	break;
		// dgs parameter
		case 10940: read_dgs_parameter(tag, str_val, &m_dgs_parameter);	break;
		// od parameter
		case 10950: read_od_parameter(tag, str_val, &m_od_parameter); break;
        // mois_bsis parameter
		case 10960: read_mobs_parameter(tag, str_val, &m_mobs_parameter); break;
		// vbc parameter
		case 10970: read_vbc_parameter(tag, str_val, &m_vbc_parameter); break;


        // pattern dimension(unit pattern)
		case 11000: read_float(tag, str_val, &m_pattern.unit_space); break;

		// arrangement
		case 11100: read_pattern_offset_from_car(tag, str_val, &m_arrangement.pattern_offset_from_car); break;
		case 11110: read_distance_between_patterns(tag, str_val, &m_arrangement.distance_between_patterns); break;
		case 11120: read_poster(tag, str_val, &m_arrangement.poster); break;
		case 11200: case 11201: case 11202: case 11203:	read_feature_points(tag, str_val, &m_feature_pts[code - 11200][0], CONTROL_POINTS_NUM);	break;
			// local points
		case 11300: case 11301: case 11302: case 11303:	read_local_points(tag, str_val, &m_local_pts[code - 11300][0], CONTROL_POINTS_NUM);	break;
		// calibrated parameter
		case 11400: case 11401: case 11402: case 11403:	read_calibrated_parameters(tag, str_val, &m_calibrated_parameters[code - 11400]); break;
		// calibrated status
		case 11500:	read_calibrated_status(tag, str_val, &m_calibrated_status); break;
		case 99999: noop; break;
		default: noop; break;
	}
}

void sanXML::read_bool(char*tag, char* src, bool* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> *dst;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_int(char* tag, char* src, int* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> *dst;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_uint(char* tag, char* src, unsigned int* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> *dst;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_float(char* tag, char* src, float* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> *dst;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);

}

void sanXML::read_xy(char* tag, char *src, XY *dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src); // to check error or exception
	try
	{
		ss >> dst->x;
		ss >> dst->y;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_xyz(char* tag, char* src, XYZ* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src); // to check error or exception
	try
	{
		ss >> dst->x;
		ss >> dst->y;
		ss >> dst->z;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_rcam_parameters(char* tag, char* src, RCAM_PARAMETERS* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->brightness;
		ss >> dst->flipx;
		ss >> dst->camview_offset.hleft;
		ss >> dst->camview_offset.hright;
		ss >> dst->camview_offset.vtop;
		ss >> dst->camview_offset.vbot;

		ss >> dst->sf;
		ss >> dst->cx;
		ss >> dst->cy;

		for (int i = 0; i < AFFINE_COEF_NUM; i++)
		{
			ss >> dst->aff[i];
		}
		for (int i = 0; i < INV_POLY_COEF_MAX_NUM; i++)
		{
			ss >> dst->invpol[i];
		}
		for (int i = 0; i < POLY_COEF_MAX_NUM; i++)
		{
			ss >> dst->pol[i];
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_vcam_pose(char* tag, char* src, POSE* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->pos.x;
		ss >> dst->pos.y;
		ss >> dst->pos.z;
		ss >> dst->ori.x;
		ss >> dst->ori.y;
		ss >> dst->ori.z;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_distance_between_patterns(char*tag, char* src, DISTANCE_BETWEEN_PATTERNS* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->vertical_1st_distance;
		ss >> dst->vertical_2nd_distance;
		ss >> dst->vertical_3rd_distance;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_poster(char* tag, char* src, POSTER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->width;
		ss >> dst->height;
		ss >> dst->radius_scale;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_pattern_offset_from_car(char*tag, char* src, PATTERN_OFFSET_FROM_CAR* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->front_pattern_offset;
		ss >> dst->right_pattern_offset;
		ss >> dst->rear_pattern_offset;
		ss >> dst->left_pattern_offset;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_projection_orthographic(char* tag, char* src, ORTHOGRAPHIC* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->left;
		ss >> dst->right;
		ss >> dst->bottom;
		ss >> dst->top;
		ss >> dst->znear;
		ss >> dst->zfar;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_projection_perspective(char* tag, char* src, PERSPECTIVE* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->fov;
		ss >> dst->znear;
		ss >> dst->zfar;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_local_points(char* tag, char* src, XYZ * dst, const int iter)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		for (int num = 0; num < iter; num += 1)
		{
			ss >> dst[num].x;
			ss >> dst[num].y;
			ss >> dst[num].z;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_feature_points(char* tag, char* src, XY* dst, const int iter)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		for (int num = 0; num < iter; num += 1)
		{
			ss >> dst[num].x;
			ss >> dst[num].y;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_layout(char* tag, char* src, LAYOUT* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->x;
		ss >> dst->y;
		ss >> dst->width;
		ss >> dst->height;
		ss >> dst->view_mode;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_view_setting(char* tag, char* src, VIEWSETTING* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->default_view;
		ss >> dst->rear_gear;
		ss >> dst->left_turn;
		ss >> dst->right_turn;
		ss >> dst->emergency;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_contour(char* tag, char* src, CONTOUR* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->roi_start_x;
		ss >> dst->roi_start_y;
		ss >> dst->roi_width;
		ss >> dst->roi_height;
		ss >> dst->contour_max_area;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_bowl3d(char* tag, char* src, BOWL3D* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->arcs_num;
		ss >> dst->start_arc_index;
		ss >> dst->start_step_index;
		ss >> dst->step_length;
		ss >> dst->nop_z;
		ss >> dst->base_radius_times;
		ss >> dst->smooth_angle;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_vehicle_information(char* tag, char* src, VEHICLE_INFORMATION* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->vehicle_id;
		ss >> dst->customer;
		ss >> dst->manufacturer;
		ss >> dst->model_name;
		ss >> dst->model_year;
		ss >> dst->vehicle_type;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_vehicle_specification(char* tag, char* src, VEHICLE_SPECIFICATION* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->length;
		ss >> dst->width;
		ss >> dst->height;
		ss >> dst->front_overhang;
		ss >> dst->wheel_base;
		ss >> dst->rear_overhang;
		ss >> dst->front_track;
		ss >> dst->rear_track;
		ss >> dst->min_steering_angle;
		ss >> dst->max_steering_angle;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_activation(char* tag, char* src, ACTIVATION* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->scene_animation;
		ss >> dst->model_animation;
		ss >> dst->model_transparency;
		ss >> dst->vbc;
		ss >> dst->pgs;
		ss >> dst->dgs;
		ss >> dst->od;
		ss >> dst->mobs;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_scene_animation_parameter(char* tag, char* src, SCENE_ANIMATION_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->scene_animation_type;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_model_animation_parameter(char* tag, char* src, MODEL_ANIMATION_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		(void) dst;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_model_lamp_parameter(char* tag, char* src, MODEL_LAMP_PARAMETER* dst)
{
	const int MAX_PARTS_NUM = 5;
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		for (int i = 0; i < MAX_PARTS_NUM; i++)
		{
			string left_part_name;
			ss >> left_part_name;
			if (left_part_name != "none")
				dst->left_lamp_names.push_back(left_part_name);
		}

		for (int i = 0; i < MAX_PARTS_NUM; i++)
		{
			string right_part_name;
			ss >> right_part_name;
			if (right_part_name != "none")
				dst->right_lamp_names.push_back(right_part_name);
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_pgs_parameter(char* tag, char* src, PGS_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->guide_max_distance;
		ss >> dst->rear_1st_distance;
		ss >> dst->rear_2nd_distance;
		ss >> dst->rear_3rd_distance;
		ss >> dst->unit_sample_width_mm;
		ss >> dst->unit_sample_length_mm;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_dgs_parameter(char* tag, char* src, DGS_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->monitoring_distance;
		ss >> dst->unit_square_length;
		ss >> dst->layout0_line_thickness;
		ss >> dst->layout1_line_thickness;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_od_parameter(char* tag, char* src, OD_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->warning_min_distance;
		ss >> dst->warning_max_distance;
		ss >> dst->circle_target_radius;
		ss >> dst->target_gradient_time_msec;
		ss >> dst->info_enable;
		ss >> dst->info_font_name;
		ss >> dst->info_font_size;
		ss >> dst->info_scale_2d;
		ss >> dst->info_scale_3d;
		ss >> dst->coast_cycles_threshold;
		ss >> dst->min_hit_streak;
		ss >> dst->duplicate_object_iou_threshold;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_mobs_parameter(char* tag, char* src, MOBS_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->mois_speed_min;
		ss >> dst->mois_speed_max;
		ss >> dst->mois_ttc_warning;
		ss >> dst->mois_ttc_monitoring;
		ss >> dst->mois_dist_warning_head;
		ss >> dst->mois_dist_warning_side;
		ss >> dst->mois_dist_monitoring_head;
		ss >> dst->mois_dist_monitoring_side;

		ss >> dst->bsis_speed_min;
		ss >> dst->bsis_speed_max;
		ss >> dst->bsis_lca_speed_min;
		ss >> dst->bsis_ttc_warning;
		ss >> dst->bsis_ttc_monitoring;
		ss >> dst->right_bsis_dist_warning_head;
		ss >> dst->right_bsis_dist_warning_side;
		ss >> dst->right_bsis_dist_warning_tail;
		ss >> dst->right_bsis_dist_monitoring_head;
		ss >> dst->right_bsis_dist_monitoring_side;
		ss >> dst->right_bsis_dist_monitoring_tail;
		ss >> dst->left_bsis_dist_warning_head;
		ss >> dst->left_bsis_dist_warning_side;
		ss >> dst->left_bsis_dist_warning_tail;
		ss >> dst->left_bsis_dist_monitoring_head;
		ss >> dst->left_bsis_dist_monitoring_side;
		ss >> dst->left_bsis_dist_monitoring_tail;

		ss >> dst->reverse_speed_min;
		ss >> dst->reverse_speed_max;
		ss >> dst->reverse_ttc_warning;
		ss >> dst->right_rear_dist_warning_side;
		ss >> dst->right_rear_dist_warning_tail;
		ss >> dst->left_rear_dist_warning_side;
		ss >> dst->left_rear_dist_warning_tail;
		ss >> dst->center_rear_dist_warning_side;
		ss >> dst->center_rear_dist_warning_near_tail;
		ss >> dst->center_rear_dist_warning_far_tail;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_vbc_parameter(char* tag, char* src, VBC_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->front_expanded_distance_mm;
		ss >> dst->right_expanded_distance_mm;
		ss >> dst->rear_expanded_distance_mm;
		ss >> dst->left_expanded_distance_mm;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_calibrated_parameters(char* tag, char* src, CALIBRATED_PARAMETER* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->K[0];
		ss >> dst->K[1];
		ss >> dst->K[2];
		ss >> dst->K[3];
		ss >> dst->K[4];
		ss >> dst->K[5];
		ss >> dst->K[6];
		ss >> dst->K[7];
		ss >> dst->K[8];
		ss >> dst->ext[0];
		ss >> dst->ext[1];
		ss >> dst->ext[2];
		ss >> dst->ext[3];
		ss >> dst->ext[4];
		ss >> dst->ext[5];
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::read_calibrated_status(char* tag, char* src, CALIBRATED_STATUS* dst)
{
	stringstream ss;
	PRE_PROCESSING(ss, src);
	try
	{
		ss >> dst->calibration_done;

		string date, time, locale;
		ss >> date;
		ss >> time;
		ss >> locale;
		dst->completion_time = date + " " + time + " " + locale;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
	POST_PROCESSING(ss, src, tag);
}

void sanXML::format_local_points_for_writing(XYZ* points /*[in]*/, int* data /*[out]*/)
{
	for (int i = 0; i < CONTROL_POINTS_NUM; i++)
	{
		data[3 * i + 0] = (int)points[i].x;
		data[3 * i + 1] = (int)points[i].y;
		data[3 * i + 2] = (int)points[i].z;
	}
}

void sanXML::format_feature_points_for_writing(XY* points/*in*/, int* data/*[out]*/)
{
	for (int i = 0; i < CONTROL_POINTS_NUM; i++)
	{
		data[2 * i + 0] = (int)points[i].x;
		data[2 * i + 1] = (int)points[i].y;
	} 
}

void sanXML::format_calibrated_parameters_for_writing(CALIBRATED_PARAMETER& calibrated_param, double* buf /*[out]*/)
{
	// 3x3 camera matrix
	buf[0] = calibrated_param.K[0];
	buf[1] = calibrated_param.K[1];
	buf[2] = calibrated_param.K[2];
	buf[3] = calibrated_param.K[3];
	buf[4] = calibrated_param.K[4];
	buf[5] = calibrated_param.K[5];
	buf[6] = calibrated_param.K[6];
	buf[7] = calibrated_param.K[7];
	buf[8] = calibrated_param.K[8];


	// 3x1 translation vector of the extrinsic parameter
	buf[9] = calibrated_param.ext[0];
	buf[10] = calibrated_param.ext[1];
	buf[11] = calibrated_param.ext[2];

	// 3x1 rotation vector(Rodrigues vector) of the extrinsic parameter
	buf[12] = calibrated_param.ext[3];
	buf[13] = calibrated_param.ext[4];
	buf[14] = calibrated_param.ext[5];
}

void sanXML::xmlUpdateCache(xmlDocPtr xml_doc_ptr, WRITING_PARAMETERS& wp, string level3_node_basename)
{
	string level3_nodename_feature = level3_node_basename + "_feature";
	string level3_nodename_local = level3_node_basename + "_local";
	string level3_nodename_calibrated = level3_node_basename + "_calibrated";
	int    data_idx = 0;

	if (xmlNodePtr node_ptr = xmlDocGetRootElement(xml_doc_ptr)) //LEVEL1(ROOT)
	{
		for (node_ptr = node_ptr->children; node_ptr != nullptr; node_ptr = node_ptr->next) //LEVEL2
		{		
			if (node_ptr->type == XML_ELEMENT_NODE)
			{
				if (strcmp((char*)node_ptr->name, "feature_points") == 0)
				{
					for (xmlNodePtr pchildren = node_ptr->children; pchildren != nullptr; pchildren = pchildren->next) //LEVEL3
					{
						if ((pchildren->type == XML_ELEMENT_NODE) && (strcmp((char*)pchildren->name, level3_nodename_feature.c_str()) == 0)) //rcamx_feature
						{
							data_idx = 0;
							for (xmlNodePtr pcchildren = pchildren->children; pcchildren != nullptr; pcchildren = pcchildren->next)
							{
								if (pcchildren->type == XML_ELEMENT_NODE)
								{
									string str_value = to_string(wp.feature_points[data_idx++]);
									xmlNodeSetContent(pcchildren, (xmlChar*)str_value.c_str());

									if (const xmlError* xml_error = xmlGetLastError())
									{
										xmlFreeDoc(xml_doc_ptr);
										xml_doc_ptr = nullptr;
										xmlCleanupParser();
										throw runtime_error(xml_error->message);
									}
									else noop;
								}
								else noop;
							}
						}
						else noop;
					}
				}
				else if (strcmp((char*)node_ptr->name, "local_points") == 0)
				{
					for (xmlNodePtr pchildren = node_ptr->children; pchildren != nullptr; pchildren = pchildren->next) //LEVEL3
					{
						if ((pchildren->type == XML_ELEMENT_NODE) && (strcmp((char*)pchildren->name, level3_nodename_local.c_str()) == 0)) // rcamx_local
						{
							data_idx = 0;
							for (xmlNodePtr pcchildren = pchildren->children; pcchildren != nullptr; pcchildren = pcchildren->next)
							{
								if (pcchildren->type == XML_ELEMENT_NODE)
								{
									string str_value = to_string(wp.local_points[data_idx++]);
									xmlNodeSetContent(pcchildren, (xmlChar*)str_value.c_str());

									if (const xmlError* xml_error = xmlGetLastError())
									{
										xmlFreeDoc(xml_doc_ptr);
										xml_doc_ptr = nullptr;
										xmlCleanupParser();
										throw runtime_error(xml_error->message);
									}
									else noop;
								}
								else noop;
							}
						}
						else noop;
					}
				}
				else if(strcmp((char*)node_ptr->name, "calibrated_parameters") == 0)
				{
					for (xmlNodePtr pchildren = node_ptr->children; pchildren != nullptr; pchildren = pchildren->next) //LEVEL3
					{
						if (pchildren->type == XML_ELEMENT_NODE && strcmp((char*)pchildren->name, "calibrated_status") == 0) // calibrated_status
						{
							data_idx = 0;
							for (xmlNodePtr pcchildren = pchildren->children; pcchildren != nullptr; pcchildren = pcchildren->next)
							{
								if (pcchildren->type == XML_ELEMENT_NODE)
								{
									xmlNodeSetContent(pcchildren, (xmlChar*)wp.calibrated_status[data_idx++].c_str());

									if (const xmlError* xml_error = xmlGetLastError())
									{
										xmlFreeDoc(xml_doc_ptr);
										xml_doc_ptr = nullptr;
										xmlCleanupParser();
										throw runtime_error(xml_error->message);
									}
									else noop;
								}
								else noop;
							}
						}
						else noop;


						if ((pchildren->type == XML_ELEMENT_NODE) && (strcmp((char*)pchildren->name, level3_nodename_calibrated.c_str()) == 0)) // rcamx_calibrated
						{
							data_idx = 0;
							for (xmlNodePtr pcchildren = pchildren->children; pcchildren != nullptr; pcchildren = pcchildren->next)
							{
								if (pcchildren->type == XML_ELEMENT_NODE)
								{
									string str_value = to_mystring(wp.calibrated_parameter[data_idx++], 30);
									xmlNodeSetContent(pcchildren, (xmlChar*)str_value.c_str());
									
									if (const xmlError* xml_error = xmlGetLastError())
									{	
										xmlFreeDoc(xml_doc_ptr);
										xml_doc_ptr = nullptr;		
										xmlCleanupParser();
										throw runtime_error(xml_error->message);
									}
									else noop;
								}
								else noop;
							}
						}
						else noop;
					}
				}
				else if (strcmp((char*)node_ptr->name, "arrangement") == 0)
				{
					string node_child_name = "poster_size";

					for (xmlNodePtr pchildren = node_ptr->children; pchildren != nullptr; pchildren = pchildren->next) //LEVEL3
					{
						if ((pchildren->type == XML_ELEMENT_NODE) && (strcmp((char*)pchildren->name, node_child_name.c_str()) == 0)) // poster_size
						{
							data_idx = 0;
							for (xmlNodePtr pcchildren = pchildren->children; pcchildren != nullptr; pcchildren = pcchildren->next)
							{
								if (pcchildren->type == XML_ELEMENT_NODE)
								{
									string str_value = to_string(wp.poster_size[data_idx++]);
									xmlNodeSetContent(pcchildren, (xmlChar*)str_value.c_str());

									if (const xmlError* xml_error = xmlGetLastError())
									{
										xmlFreeDoc(xml_doc_ptr);
										xml_doc_ptr = nullptr;
										xmlCleanupParser();
										throw runtime_error(xml_error->message);
									}
									else noop;
								}
								else noop;
							}
						}
						else noop;
					}
				}
				else noop;
			}
			else noop;
		}
	}
	else
	{	
		xmlFreeDoc(xml_doc_ptr);
		xml_doc_ptr = nullptr;	
		xmlCleanupParser();
		throw runtime_error(string(xmlGetLastError()->message));
	}
}

void sanXML::create_output_calibration_file(string file_path)
{
	xmlDocPtr doc = nullptr;
	xmlNodePtr root_node = nullptr, node = nullptr, childNode = nullptr;
	xmlNodePtr comment = nullptr;

	doc = xmlNewDoc(BAD_CAST "1.0");
	root_node = xmlNewNode(nullptr, BAD_CAST "svm");
	xmlDocSetRootElement(doc, root_node);

	//--------------------------------------------------------
	// arrangement
	//--------------------------------------------------------
	node = xmlNewNode(nullptr, BAD_CAST "arrangement");
	childNode = xmlNewNode(nullptr, BAD_CAST "poster_size");
	
	comment = xmlNewComment(BAD_CAST " poster size in millimeter (width, height), scale factor for Bird Eye View with no unit (radius_scale) ");
	xmlAddChild(childNode, comment);

	xmlNewChild(childNode, nullptr, BAD_CAST "width", BAD_CAST "0");
	xmlNewChild(childNode, nullptr, BAD_CAST "height", BAD_CAST "0");
	xmlNewChild(childNode, nullptr, BAD_CAST "radius_scale", BAD_CAST "0.0");
	xmlAddChild(node, childNode);
	xmlAddChild(root_node, node);

	//--------------------------------------------------------
	// calibrated_parameters
	//--------------------------------------------------------
	node = xmlNewNode(nullptr, BAD_CAST "calibrated_parameters");

	// calibrated_status
	childNode = xmlNewNode(nullptr, BAD_CAST "calibrated_status");
	xmlNewChild(childNode, nullptr, BAD_CAST "calibration_done", BAD_CAST "0");
	xmlNewChild(childNode, nullptr, BAD_CAST "completion_time", BAD_CAST "1900-01-01 00:00:00 UTC+09:00");
	xmlAddChild(node, childNode);

	// calibrated_data
	comment = xmlNewComment(BAD_CAST " Calibrated Intrinsic and Extrinsic parameters ");
	xmlAddChild(node, comment);
	for (int i = 0; i < SVM_CAMERAS_NUM; i++)
	{
		string childName = "rcam" + std::to_string(i) + "_calibrated";
		childNode = xmlNewNode(nullptr, BAD_CAST (char*) & childName.c_str()[0]);

		comment = xmlNewComment(BAD_CAST " Intrinsic parameter(3x3 camera matrix): m00(fmu), m01(sf), m02(cx), m11(fmv), m12(cy) ");
		xmlAddChild(childNode, comment);
		for (int k = 0; k < 9; k++)
		{
			string cchildName = "m" + std::to_string(k / 3) + std::to_string(k % 3);
			xmlNewChild(childNode, nullptr, BAD_CAST(char*) & cchildName.c_str()[0], BAD_CAST to_mystring(0.0, 30).c_str());
		}

		comment = xmlNewComment(BAD_CAST " Extrinsic parameter (3x1 translation vector (px,py,pz), 3x1 rotation (Rodrigues rotation rx,ry,rz) vector ");
		xmlAddChild(childNode, comment);
		xmlNewChild(childNode, nullptr, BAD_CAST "px", BAD_CAST to_mystring(0.0, 30).c_str());
		xmlNewChild(childNode, nullptr, BAD_CAST "py", BAD_CAST to_mystring(0.0, 30).c_str());
		xmlNewChild(childNode, nullptr, BAD_CAST "pz", BAD_CAST to_mystring(0.0, 30).c_str());
		xmlNewChild(childNode, nullptr, BAD_CAST "rx", BAD_CAST to_mystring(0.0, 30).c_str());
		xmlNewChild(childNode, nullptr, BAD_CAST "ry", BAD_CAST to_mystring(0.0, 30).c_str());
		xmlNewChild(childNode, nullptr, BAD_CAST "rz", BAD_CAST to_mystring(0.0, 30).c_str());

		xmlAddChild(node, childNode);
	}
	xmlAddChild(root_node, node);


	//--------------------------------------------------------
	// save file
	//--------------------------------------------------------
	char* full_file_path = (char*)&file_path.c_str()[0];
	xmlSaveFormatFileEnc(full_file_path, doc, "UTF-8", 1);
	xmlFreeDoc(doc);
}

//----------------------check range of all parameters--------------------------

void sanXML::check_range_all_parameters()
{
	try
	{
		// common
		check_range_system_parameters();
		check_range_real_vehicle_parameters();

#ifdef CALIBRATION_APP
		check_range_calibration_parameters();
		check_range_image_processing_parameters();
		check_range_space_grid_parameters();
		check_range_unit_pattern_parameters();
		check_range_arrangement_parameters();
		if (m_calibration_type == MANUAL_CALIBRATION)
			check_range_feature_points_parameters();
		else noop;
#endif // CALIBRATION_APP

#ifdef VIEWER_APP
		check_range_virtual_camera_parameters();
		check_range_projection_parameters();
		check_range_layout_view_parameters();
		check_range_camview_parameters();
		check_range_vehicle_model_parameters();
		check_range_function_parameters();
#endif // VIEWER_APP

	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

#define CHECK_IS_INTEGER(x) ((x) - (int)(x) <= FLT_EPSILON)

//----------------------check range of system parameters--------------------------
bool sanXML::check_range_system_parameters()
{
	bool is_error = false;
	string error_msg = "wrong system_parameter[";

	try
	{
		if (m_svm_cameras_num != SVM_CAMERAS_NUM || !CHECK_IS_INTEGER(m_svm_cameras_num))
		{
			is_error = true;
			error_msg += " cameras_number must be integer " + std::to_string(SVM_CAMERAS_NUM) + "!";
		}
		else noop;

		if (m_svm_patterns_num < 4 || m_svm_patterns_num > (int)(PATTERN_MAX_NUM / 2) || !CHECK_IS_INTEGER(m_svm_patterns_num))
		{
			is_error = true;
			error_msg += " patterns_number must be integer no fewer than 4 and no greater than " + std::to_string(PATTERN_MAX_NUM / 2) + "!";
		}
		else noop;

		string resolution_err_msg = check_range_resolution_parameters();
		if (resolution_err_msg.length())
		{
			is_error = true;
			error_msg += resolution_err_msg;
		}

		// finish and check if error is existed
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

string sanXML::check_range_resolution_parameters()
{
	bool is_error = false;
	string error_msg = "";

	if (m_resolution.display.width <= 0 || !CHECK_IS_INTEGER(m_resolution.display.width))
	{
		is_error = true;
		error_msg += " display_resolution<width>,";
	}
	else noop;

	if (m_resolution.display.height <= 0 || !CHECK_IS_INTEGER(m_resolution.display.height))
	{
		is_error = true;
		error_msg += " display_resolution<height>,";
	}
	else noop;

	if (m_resolution.image.width <= 0 || !CHECK_IS_INTEGER(m_resolution.image.width))
	{
		is_error = true;
		error_msg += " image_resolution<width>,";
	}
	else noop;

	if (m_resolution.image.height <= 0 || !CHECK_IS_INTEGER(m_resolution.image.height))
	{
		is_error = true;
		error_msg += " image_resolution<height>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be positive integer number!";
	}
	else noop;

	return error_msg;
}

//------------------check range of real_vehicle parameters------------------------
bool sanXML::check_range_real_vehicle_parameters()
{
	bool is_error = false;
	string error_msg = "wrong real_vehicle[";

	try
	{
		// check valid positive values
		if (m_vehicle_spec.length <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<length>,";
		}
		else noop;

		if (m_vehicle_spec.width <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<width>,";
		}
		else noop;

		if (m_vehicle_spec.height <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<height>,";
		}
		else noop;

		if (m_vehicle_spec.wheel_base <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<wheel_base>,";
		}
		else noop;

		if (m_vehicle_spec.front_overhang <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<front_overhang>,";
		}
		else noop;

		if (m_vehicle_spec.rear_overhang <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<rear_overhang>,";
		}
		else noop;

		if (m_vehicle_spec.front_track <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<front_track>,";
		}
		else noop;

		if (m_vehicle_spec.rear_track <= 0.0f)
		{
			is_error = true;
			error_msg += " vehicle_specification<rear_track>,";
		}
		else noop;

		if (is_error)
		{
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be positive float number!";
		}

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//--------------------------------------------------------------------------------
//						CALIBRATION_APP 
//--------------------------------------------------------------------------------

//-----------------check range of calibration parameters--------------------------
bool sanXML::check_range_calibration_parameters()
{
	bool is_error = false;
	string error_msg = "wrong calibration_parameter[";

	try
	{
		if (m_calibration_type < 0 || m_calibration_type > 1 || !CHECK_IS_INTEGER(m_calibration_type))
		{
			is_error = true;
			error_msg += " calibration_type must be integer 0 or 1!";
		}
		else noop;

		// finish and check if error is existed
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//-----------------check range of image processing parameters---------------------
bool sanXML::check_range_image_processing_parameters()
{
	bool is_error = false;
	string error_msg = "wrong image_processing[";

	try
	{
		//------------------------------------------------------------------
		// check contour

		if (m_contour.roi_start_x < 0 || m_contour.roi_start_x > (int)m_resolution.image.width)
		{
			is_error = true;
			error_msg += " contour<roi_start_x> must be integer in range 0 ~ " + std::to_string(m_resolution.image.width) + "!";
		}
		else noop;

		if (m_contour.roi_start_y < 0 || m_contour.roi_start_y > (int)m_resolution.image.height)
		{
			is_error = true;
			error_msg += " contour<roi_start_y> must be integer in range 0 ~ " + std::to_string(m_resolution.image.height) + "!";
		}
		else noop;

		if (m_contour.roi_width < 0 || m_contour.roi_width > (int)m_resolution.image.width)
		{
			is_error = true;
			error_msg += " contour<roi_width> must be integer in range 0 ~ " + std::to_string(m_resolution.image.width) + "!";
		}
		else noop;

		if (m_contour.roi_height < 0 || m_contour.roi_height > (int)m_resolution.image.height)
		{
			is_error = true;
			error_msg += " contour<roi_height> must be integer in range 0 ~ " + std::to_string(m_resolution.image.height) + "!";
		}
		else noop;

		if (m_contour.contour_max_area <= 0)
		{
			is_error = true;
			error_msg += " contour<contour_max_area> must be positive integer!";
		}
		else noop;


		if ((m_contour.roi_start_x + m_contour.roi_width) > (int)m_resolution.image.width)
		{
			is_error = true;
			error_msg += " contour<roi> is horizontally image overflow!";
		}
		else noop;

		if ((m_contour.roi_start_y + m_contour.roi_height) > (int)m_resolution.image.height)
		{
			is_error = true;
			error_msg += " contour<roi> is vertically image overflow!";
		}
		else noop;
		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";
		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//------------------check range of space grid parameters--------------------------
bool sanXML::check_range_space_grid_parameters()
{
	bool is_error = false;
	string error_msg = "wrong space_grid[";

	try
	{
		//------------------------------------------------------------------
		// check bowl3D

		if ((m_grid.arcs_num <= 0) || (m_grid.arcs_num % 2 != 0) || !CHECK_IS_INTEGER(m_grid.arcs_num))
		{
			is_error = true;
			error_msg += " bowl3D<arcs_num> must be even positive integer number!";
		}
		else noop;

		if (m_grid.start_arc_index < 0 || !CHECK_IS_INTEGER(m_grid.start_arc_index))
		{
			is_error = true;
			error_msg += " bowl3D<start_arc_index> must be 0-based non-negative integer number!";
		}
		else noop;

		if (m_grid.start_step_index < 1 || !CHECK_IS_INTEGER(m_grid.start_step_index))
		{
			is_error = true;
			error_msg += " bowl3D<start_step_index> must be 1-based positive integer number!";
		}
		else noop;

		if (m_grid.step_length <= FLT_EPSILON)
		{
			is_error = true;
			error_msg += " bowl3D<step_length> must be positive number!";
		}
		else noop;

		if (m_grid.nop_z <= 0 || !CHECK_IS_INTEGER(m_grid.nop_z))
		{
			is_error = true;
			error_msg += " bowl3D<nop_z> must be positive integer number!";
		}
		else noop;

		if (m_grid.base_radius_times <= 0)
		{
			is_error = true;
			error_msg += " bowl3D<base_radius_times> must be positive number!";
		}
		else noop;

		if (m_grid.smooth_angle <= 0)
		{
			is_error = true;
			error_msg += " bowl3D<smooth_angle> must be positive number!";
		}
		else noop;

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";
		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//------------------check range of unit_pattern_dimension parameters--------------
bool sanXML::check_range_unit_pattern_parameters()
{
	bool is_error = false;
	string error_msg = "wrong unit_pattern_dimension[";

	try
	{
		if (m_pattern.unit_space <= 0.0f)
		{
			is_error = true;
			error_msg += " unit_space,";
		}
		else noop;

		if (is_error)
		{
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be positive float number!";
		}

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//--------------------check range of arrangement parameters-----------------------
bool sanXML::check_range_arrangement_parameters()
{
	bool is_error = false;
	string error_msg = "wrong arrangement[";

	try
	{
		// check valid non-negative values
		bool is_error_non_negative = false;

		if (m_arrangement.pattern_offset_from_car.front_pattern_offset < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " pattern_offset_from_car<front_pattern_offset>,";
		}
		else noop;

		if (m_arrangement.pattern_offset_from_car.right_pattern_offset < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " pattern_offset_from_car<right_pattern_offset>,";
		}
		else noop;

		if (m_arrangement.pattern_offset_from_car.rear_pattern_offset < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " pattern_offset_from_car<rear_pattern_offset>,";
		}
		else noop;

		if (m_arrangement.pattern_offset_from_car.left_pattern_offset < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " pattern_offset_from_car<left_pattern_offset>,";
		}
		else noop;

		if (m_arrangement.distance_between_patterns.vertical_2nd_distance < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " distance_between_patterns<vertical_2nd_distance>,";
		}
		else noop;

		if (m_arrangement.distance_between_patterns.vertical_3rd_distance < 0.0f)
		{
			is_error_non_negative = true;
			error_msg += " distance_between_patterns<vertical_3rd_distance>,";
		}
		else noop;

		if (is_error_non_negative)
		{
			is_error = true;
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be non-negative float number!";
		}

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//--------------------check range of feature_points parameters--------------------
bool sanXML::check_range_feature_points_parameters()
{
	bool is_error = false;
	string error_msg = "wrong feature_points[";

	try
	{
		// m_feature_pts[SVM_CAMERAS_NUM][CONTROL_POINTS_NUM]

		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			for (int pointID = 0; pointID < CONTROL_POINTS_NUM; pointID++)
			{
				if (m_feature_pts[camID][pointID].x < 0.0f || m_feature_pts[camID][pointID].x > m_resolution.image.width)
				{
					is_error = true;
					error_msg += " rcam" + to_string(camID) + "_feature" + "<x" + to_string(pointID) + ">,";
				}

				if (m_feature_pts[camID][pointID].y < 0.0f || m_feature_pts[camID][pointID].y > m_resolution.image.height)
				{
					is_error = true;
					error_msg += " rcam" + to_string(camID) + "_feature" + "<y" + to_string(pointID) + ">,";
				}
			}
		}

		if (is_error)
		{
			// finish and check if error is existed
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " is out of range 0~image_resolution!";
		}

		//------------------------------------------------------------------
		// end checking, throw error if existing

		error_msg += " ]";
		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//--------------------------------------------------------------------------------
//						VIEWER_APP 
//--------------------------------------------------------------------------------

//----------------check range of virtual camera parameters------------------------
bool sanXML::check_range_virtual_camera_parameters()
{
	bool is_error = false;
	string error_msg = "wrong virtual_camera[";

	try
	{
		for (int i = 0; i < SVM_CAMERAS_NUM + 1; i++)
		{

			int camID = (i < SVM_CAMERAS_NUM) ? i : 10;

			if (m_vcam[camID].ori.x > 180.0f || m_vcam[camID].ori.x < -180.0f)
			{
				is_error = true;
				error_msg += " vcam[" + to_string(camID) + "]<X>,";
			}
			else noop;

			if (m_vcam[camID].ori.y > 180.0f || m_vcam[camID].ori.y < -180.0f)
			{
				is_error = true;
				error_msg += " vcam[" + to_string(camID) + "]<Y>,";
			}
			else noop;

			if (m_vcam[camID].ori.z > 180.0f || m_vcam[camID].ori.z < -180.0f)
			{
				is_error = true;
				error_msg += " vcam[" + to_string(camID) + "]<Z>,";
			}
			else noop;
		}

		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " axis rotation angle must be float in range (-180,180)! ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//------------------check range of projection parameters--------------------------
bool sanXML::check_range_projection_parameters()
{
	bool is_error = false;
	string error_msg = "wrong projection[";

	try
	{
		if (m_orthogrphic.znear <= 0)
		{
			is_error = true;
			error_msg += " orthographic<znear>,";
		}
		else noop;

		if (m_orthogrphic.zfar <= 0)
		{
			is_error = true;
			error_msg += " orthographic<zfar>,";
		}
		else noop;

		if (m_perspective.znear <= 0)
		{
			is_error = true;
			error_msg += " perspective<znear>,";
		}
		else noop;

		if (m_perspective.zfar <= 0)
		{
			is_error = true;
			error_msg += " perspective<zfar>,";
		}
		else noop;

		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be positive number! ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//------------------check range of layout view parameters-------------------------
bool sanXML::check_range_layout_view_parameters()
{
	bool is_error = false;
	string error_msg = "wrong layout_view[";

	try
	{
		//------------------------------------------------------------------
		// check layout starting position
		bool is_error_starting_position = false;

		if (m_layout[0].x < 0 || !CHECK_IS_INTEGER(m_layout[0].x))
		{
			is_error_starting_position = true;
			error_msg += " layout0<x>,";
		}
		else noop;

		if (m_layout[0].y < 0 || !CHECK_IS_INTEGER(m_layout[0].y))
		{
			is_error_starting_position = true;
			error_msg += " layout0<y>,";
		}
		else noop;

		if (m_layout[1].x < 0 || !CHECK_IS_INTEGER(m_layout[1].x))
		{
			is_error_starting_position = true;
			error_msg += " layout1<x>,";
		}
		else noop;

		if (m_layout[1].y < 0 || !CHECK_IS_INTEGER(m_layout[1].y))
		{
			is_error_starting_position = true;
			error_msg += " layout1<y>,";
		}
		else noop;

		if (is_error_starting_position)
		{
			is_error = true;
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be non-negative integer number!";
		}
		else noop;

		//------------------------------------------------------------------
		// check layout size
		bool is_error_size = false;

		if (m_layout[0].width <= 0 || !CHECK_IS_INTEGER(m_layout[0].width))
		{
			is_error_size = true;
			error_msg += " layout0<width>,";
		}
		else noop;

		if (m_layout[0].height <= 0 || !CHECK_IS_INTEGER(m_layout[0].height))
		{
			is_error_size = true;
			error_msg += " layout0<height>,";
		}
		else noop;

		if (m_layout[1].width <= 0 || !CHECK_IS_INTEGER(m_layout[1].width))
		{
			is_error_size = true;
			error_msg += " layout1<width>,";
		}
		else noop;

		if (m_layout[1].height <= 0 || !CHECK_IS_INTEGER(m_layout[1].height))
		{
			is_error_size = true;
			error_msg += " layout1<height>,";
		}
		else noop;

		if (is_error_size)
		{
			is_error = true;
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be positive integer number!";
		}
		else noop;

		//------------------------------------------------------------------
		// check layout overlapping
		bool is_error_overlapping = false;

		if ((m_layout[0].x + m_layout[0].width) > m_resolution.display.width)
		{
			is_error_overlapping = true;
			error_msg += " layout0 is horizontally screen overflow!";
		}
		else noop;

		if ((m_layout[0].y + m_layout[0].height) > m_resolution.display.height)
		{
			is_error_overlapping = true;
			error_msg += " layout0 is vertically screen overflow!";
		}
		else noop;

		if ((m_layout[1].x + m_layout[1].width) > m_resolution.display.width)
		{
			is_error_overlapping = true;
			error_msg += " layout1 is horizontally screen overflow!";
		}
		else noop;

		if ((m_layout[1].y + m_layout[1].height) > m_resolution.display.height)
		{
			is_error_overlapping = true;
			error_msg += " layout1 is vertically screen overflow!";
		}
		else noop;

		if ((m_layout[0].x + m_layout[0].width) > m_layout[1].x)
		{
			is_error_overlapping = true;
			error_msg += " layout0 and layout1 are horizontally overlapped!";
		}
		else noop;

		if (is_error_overlapping) is_error = true; else noop;

		//------------------------------------------------------------------
		// check view_mode
		bool is_error_view_mode = false;

		if (m_layout[0].view_mode != VIEWMODE::TOPVIEW3D || !CHECK_IS_INTEGER(m_layout[0].view_mode))
		{
			is_error_view_mode = true;
			error_msg += " layout0<view_mode> must be integer 0 (TOPVIEW3D)!";
		}

		if ((int)m_layout[1].view_mode < (int)VIEWMODE::CAMVIEW3D_FRONT || (int)m_layout[1].view_mode > (int)VIEWMODE::CAMVIEW2D_LEFT || !CHECK_IS_INTEGER(m_layout[1].view_mode))
		{
			is_error_view_mode = true;
			error_msg += " layout1<view_mode> must be integer in range 1(CAMVIEW3D_FRONT) ~ 9(CAMVIEW2D_ADD0)!";
		}

		if (is_error_view_mode) is_error = true; else noop;

		//------------------------------------------------------------------
		// check view setting for layout mode
		bool is_error_view_setting_layout_mode = false;

		if (m_view_setting.default_view < (int)LAYOUTMODE::TOPVIEW_FRONT_3D || m_view_setting.default_view > (int)LAYOUTMODE::TOPVIEW_ADD0_2D || !CHECK_IS_INTEGER(m_view_setting.default_view))
		{
			is_error_view_setting_layout_mode = true;
			error_msg += " view_setting<default_view>,";
		}

		if (m_view_setting.rear_gear < (int)LAYOUTMODE::TOPVIEW_FRONT_3D || m_view_setting.rear_gear >(int)LAYOUTMODE::TOPVIEW_ADD0_2D || !CHECK_IS_INTEGER(m_view_setting.rear_gear))
		{
			is_error_view_setting_layout_mode = true;
			error_msg += " view_setting<rear_gear>,";
		}

		if (m_view_setting.left_turn < (int)LAYOUTMODE::TOPVIEW_FRONT_3D || m_view_setting.left_turn >(int)LAYOUTMODE::TOPVIEW_ADD0_2D || !CHECK_IS_INTEGER(m_view_setting.left_turn))
		{
			is_error_view_setting_layout_mode = true;
			error_msg += " view_setting<left_turn>,";
		}

		if (m_view_setting.right_turn < (int)LAYOUTMODE::TOPVIEW_FRONT_3D || m_view_setting.right_turn >(int)LAYOUTMODE::TOPVIEW_ADD0_2D || !CHECK_IS_INTEGER(m_view_setting.right_turn))
		{
			is_error_view_setting_layout_mode = true;
			error_msg += " view_setting<right_turn>,";
		}

		if (m_view_setting.emergency < (int)LAYOUTMODE::TOPVIEW_FRONT_3D || m_view_setting.emergency >(int)LAYOUTMODE::TOPVIEW_ADD0_2D || !CHECK_IS_INTEGER(m_view_setting.emergency))
		{
			is_error_view_setting_layout_mode = true;
			error_msg += " view_setting<emergency>,";
		}

		if (is_error_view_setting_layout_mode)
		{
			is_error = true;
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " must be integer in range 1(TOPVIEW_FRONT_3D) ~ 9(TOPVIEW_ADD0_2D)!";
		}
		else noop;

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";
		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//------------------check range of cam view parameters----------------------------
bool sanXML::check_range_camview_parameters()
{
	bool is_error = false;
	string error_msg = "";

	try
	{
		// for real cam

		error_msg = "wrong real_cam[";

		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			//------------------------------------------------------------------
			// check camview_offset integer

			bool is_error_integer = false;

			if ((m_rcam[camID].camview_offset.hleft < 0) || !CHECK_IS_INTEGER(m_rcam[camID].camview_offset.hleft))
			{
				is_error_integer = true;
				error_msg += " rcam[" + to_string(camID) + "]<hleft>,";
			}
			else noop;

			if ((m_rcam[camID].camview_offset.hright < 0) || !CHECK_IS_INTEGER(m_rcam[camID].camview_offset.hright))
			{
				is_error_integer = true;
				error_msg += " rcam[" + to_string(camID) + "]<hright>,";
			}
			else noop;

			if ((m_rcam[camID].camview_offset.vtop < 0) || !CHECK_IS_INTEGER(m_rcam[camID].camview_offset.vtop))
			{
				is_error_integer = true;
				error_msg += " rcam[" + to_string(camID) + "]<vtop>,";
			}
			else noop;

			if ((m_rcam[camID].camview_offset.vbot < 0) || !CHECK_IS_INTEGER(m_rcam[camID].camview_offset.vbot))
			{
				is_error_integer = true;
				error_msg += " rcam[" + to_string(camID) + "]<vbot>,";
			}
			else noop;

			if (is_error_integer)
			{
				is_error = true;
				if (error_msg.length())
					error_msg.pop_back(); // remove last character ","

				error_msg += " must be positive integer number!";
			}

			//------------------------------------------------------------------
			// check camview_offset range

			bool is_error_range = false;

			if (m_rcam[camID].camview_offset.hleft > m_resolution.image.width)
			{
				is_error_range = true;
				error_msg += " rcam[" + to_string(camID) + "]<hleft>,";
			}
			else noop;

			if (m_rcam[camID].camview_offset.hright > m_resolution.image.width)
			{
				is_error_range = true;
				error_msg += " rcam[" + to_string(camID) + "]<hright>,";
			}
			else noop;

			if (m_rcam[camID].camview_offset.vtop > m_resolution.image.height)
			{
				is_error_range = true;
				error_msg += " rcam[" + to_string(camID) + "]<vtop>,";
			}
			else noop;

			if (m_rcam[camID].camview_offset.vbot > m_resolution.image.height)
			{
				is_error_range = true;
				error_msg += " rcam[" + to_string(camID) + "]<vbot>,";
			}
			else noop;

			if (is_error_range)
			{
				is_error = true;
				if (error_msg.length())
					error_msg.pop_back(); // remove last character ","

				error_msg += " is out of image resolution range!";
			}

			//------------------------------------------------------------------
			// end checking, throw error if existing
			error_msg += " ]";

			if (is_error)
			{
				throw runtime_error(error_msg);
			}
			else noop;
		}


		// for additional cam

		error_msg = "wrong additional_cam[";

		for (int camID = 0; camID < ADD_CAMERAS_NUM; camID++)
		{
			//------------------------------------------------------------------
			// check camview_offset integer

			bool is_error_integer = false;

			if ((m_acam[camID].camview_offset.hleft < 0) || !CHECK_IS_INTEGER(m_acam[camID].camview_offset.hleft))
			{
				is_error_integer = true;
				error_msg += " acam[" + to_string(camID) + "]<hleft>,";
			}
			else noop;

			if ((m_acam[camID].camview_offset.hright < 0) || !CHECK_IS_INTEGER(m_acam[camID].camview_offset.hright))
			{
				is_error_integer = true;
				error_msg += " acam[" + to_string(camID) + "]<hright>,";
			}
			else noop;

			if ((m_acam[camID].camview_offset.vtop < 0) || !CHECK_IS_INTEGER(m_acam[camID].camview_offset.vtop))
			{
				is_error_integer = true;
				error_msg += " acam[" + to_string(camID) + "]<vtop>,";
			}
			else noop;

			if ((m_acam[camID].camview_offset.vbot < 0) || !CHECK_IS_INTEGER(m_acam[camID].camview_offset.vbot))
			{
				is_error_integer = true;
				error_msg += " acam[" + to_string(camID) + "]<vbot>,";
			}
			else noop;

			if (is_error_integer)
			{
				is_error = true;
				if (error_msg.length())
					error_msg.pop_back(); // remove last character ","

				error_msg += " must be positive integer number!";
			}

			//------------------------------------------------------------------
			// check camview_offset range

			bool is_error_range = false;

			if (m_acam[camID].camview_offset.hleft > m_resolution.image.width)
			{
				is_error_range = true;
				error_msg += " acam[" + to_string(camID) + "]<hleft>,";
			}
			else noop;

			if (m_acam[camID].camview_offset.hright > m_resolution.image.width)
			{
				is_error_range = true;
				error_msg += " acam[" + to_string(camID) + "]<hright>,";
			}
			else noop;

			if (m_acam[camID].camview_offset.vtop > m_resolution.image.height)
			{
				is_error_range = true;
				error_msg += " acam[" + to_string(camID) + "]<vtop>,";
			}
			else noop;

			if (m_acam[camID].camview_offset.vbot > m_resolution.image.height)
			{
				is_error_range = true;
				error_msg += " acam[" + to_string(camID) + "]<vbot>,";
			}
			else noop;

			if (is_error_range)
			{
				is_error = true;
				if (error_msg.length())
					error_msg.pop_back(); // remove last character ","

				error_msg += " is out of image resolution range!";
			}

			//------------------------------------------------------------------
			// end checking, throw error if existing
			error_msg += " ]";

			if (is_error)
			{
				throw runtime_error(error_msg);
			}
			else noop;
		}

	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//----------------check range of vehicle model parameters-------------------------
bool sanXML::check_range_vehicle_model_parameters()
{
	bool is_error = false;
	string error_msg = "wrong vehicle_model[";

	try
	{
		//------------------------------------------------------------------
		// check model_scale

		bool is_error_model_scale = false;

		if (m_model.scale.x < 0.0f)
		{
			is_error_model_scale = true;
			error_msg += " model_scale<x>,";
		}
		else noop;

		if (m_model.scale.y < 0.0f)
		{
			is_error_model_scale = true;
			error_msg += " model_scale<y>,";
		}
		else noop;

		if (m_model.scale.z < 0.0f)
		{
			is_error_model_scale = true;
			error_msg += " model_scale<z>,";
		}
		else noop;

		if (is_error_model_scale)
		{
			is_error = true;
			if (error_msg.length())
				error_msg.pop_back(); // remove last character ","

			error_msg += " should be non-negative number!";
		}

		//------------------------------------------------------------------
		// end checking, throw error if existing
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

//-------------------check range of function parameters---------------------------
bool sanXML::check_range_function_parameters()
{
	bool is_error = false;
	string error_msg = "wrong function[";

	try
	{
		// check activation range
		string activation_err_msg = check_range_activation_parameters();
		if (activation_err_msg.length())
		{
			is_error = true;
			error_msg += activation_err_msg;
		}
		else noop;

		// check scene_animation range
		string scene_animation_err_msg = check_range_scene_animation_parameters();
		if (scene_animation_err_msg.length())
		{
			is_error = true;
			error_msg += scene_animation_err_msg;
		}
		else noop;

		// check model_animation range
		string model_animation_err_msg = check_range_model_animation_parameters();
		if (model_animation_err_msg.length())
		{
			is_error = true;
			error_msg += model_animation_err_msg;
		}
		else noop;

		// check model_transparency range
		string model_transparency_err_msg = check_range_model_transparency_parameters();
		if (model_transparency_err_msg.length())
		{
			is_error = true;
			error_msg += model_transparency_err_msg;
		}
		else noop;

		// check model_lamp range
		string model_lamp_err_msg = check_range_model_lamp_parameters();
		if (model_lamp_err_msg.length())
		{
			is_error = true;
			error_msg += model_lamp_err_msg;
		}
		else noop;

		// check vbc range
		string vbc_err_msg = check_range_vbc_parameters();
		if (vbc_err_msg.length())
		{
			is_error = true;
			error_msg += vbc_err_msg;
		}
		else noop;

		// check pgs range
		string pgs_err_msg = check_range_pgs_parameters();
		if (pgs_err_msg.length())
		{
			is_error = true;
			error_msg += pgs_err_msg;
		}
		else noop;

		// check dgs range
		string dgs_err_msg = check_range_dgs_parameters();
		if (dgs_err_msg.length())
		{
			is_error = true;
			error_msg += dgs_err_msg;
		}
		else noop;

		// check od range
		string od_err_msg = check_range_od_parameters();
		if (od_err_msg.length())
		{
			is_error = true;
			error_msg += od_err_msg;
		}
		else noop;
		
		// check mobs range
		string mobs_err_msg = check_range_mobs_parameters();
		if (mobs_err_msg.length())
		{
			is_error = true;
			error_msg += mobs_err_msg;
		}
		else noop;

		// finish and check if error is existed
		error_msg += " ]";

		if (is_error)
		{
			throw runtime_error(error_msg);
		}
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(string(e.what()));
	}

	return is_error;
}

string sanXML::check_range_activation_parameters()
{
	bool is_error = false;
	string error_msg = "";

	if ((int)m_activation.scene_animation < 0 || (int)m_activation.scene_animation > 1 || !CHECK_IS_INTEGER(m_activation.scene_animation))
	{
		is_error = true;
		error_msg += " activation<scene_animation>,";
	}
	else noop;

	if ((int)m_activation.model_animation < 0 || (int)m_activation.model_animation > 1 || !CHECK_IS_INTEGER(m_activation.model_animation))
	{
		is_error = true;
		error_msg += " activation<model_animation>,";
	}
	else noop;

	if ((int)m_activation.model_transparency < 0 || (int)m_activation.model_transparency > 1 || !CHECK_IS_INTEGER(m_activation.model_transparency))
	{
		is_error = true;
		error_msg += " activation<model_transparency>,";
	}
	else noop;

	if ((int)m_activation.vbc < 0 || (int)m_activation.vbc > 1 || !CHECK_IS_INTEGER(m_activation.vbc))
	{
		is_error = true;
		error_msg += " activation<vbc>,";
	}
	else noop;

	if ((int)m_activation.pgs < 0 || (int)m_activation.pgs > 1 || !CHECK_IS_INTEGER(m_activation.pgs))
	{
		is_error = true;
		error_msg += " activation<pgs>,";
	}
	else noop;

	if ((int)m_activation.dgs < 0 || (int)m_activation.dgs > 1 || !CHECK_IS_INTEGER(m_activation.dgs))
	{
		is_error = true;
		error_msg += " activation<dgs>,";
	}
	else noop;

	if ((int)m_activation.od < 0 || (int)m_activation.od > 1 || !CHECK_IS_INTEGER(m_activation.od))
	{
		is_error = true;
		error_msg += " activation<od>,";
	}
	else noop;

	if ((int)m_activation.mobs < 0 || (int)m_activation.mobs > 1 || !CHECK_IS_INTEGER(m_activation.mobs))
	{
		is_error = true;
		error_msg += " activation<mobs>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be boolean 0 or 1!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_scene_animation_parameters()
{
	bool is_error = false;
	string error_msg = "";

	if (m_scene_animation_parameter.scene_animation_type < 0 || m_scene_animation_parameter.scene_animation_type > 2 || !CHECK_IS_INTEGER(m_scene_animation_parameter.scene_animation_type))
	{
		is_error = true;
		error_msg += " scene_animation_parameter<scene_animation_type>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be integer in range 0~2!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_model_animation_parameters()
{
	//bool is_error = false;
	string error_msg = "";

	return error_msg;
}

string sanXML::check_range_model_transparency_parameters()
{
	bool is_error = false;
	string error_msg = "";

	if (m_model_transparency_parameter.rate < 0.0f || m_model_transparency_parameter.rate > 1.0f)
	{
		is_error = true;
		error_msg += " model_transparency_parameter<rate>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be float in range 0.0~1.0!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_model_lamp_parameters()
{
	//bool is_error = false;
	string error_msg = "";

	return error_msg;
}

string sanXML::check_range_vbc_parameters()
{
	bool is_error = false;
	string error_msg = "";

	if (m_vbc_parameter.front_expanded_distance_mm < 0.0f)
	{
		is_error = true;
		error_msg += " vbc_parameter<front_expanded_distance>,";
	}
	else noop;

	if (m_vbc_parameter.right_expanded_distance_mm < 0.0f)
	{
		is_error = true;
		error_msg += " vbc_parameter<right_expanded_distance>,";
	}
	else noop;

	if (m_vbc_parameter.rear_expanded_distance_mm < 0.0f)
	{
		is_error = true;
		error_msg += " vbc_parameter<rear_expanded_distance>,";
	}
	else noop;

	if (m_vbc_parameter.left_expanded_distance_mm < 0.0f)
	{
		is_error = true;
		error_msg += " vbc_parameter<left_expanded_distance>,";
	}
	else noop;
	
	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative float number!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_pgs_parameters()
{
	bool is_error = false;
	string error_msg = "";

	// check valid number
	if (m_pgs_parameter.guide_max_distance < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<guide_max_distance>,";
	}
	else noop;

	if (m_pgs_parameter.rear_1st_distance < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<rear_1st_distance>,";
	}
	else noop;

	if (m_pgs_parameter.rear_2nd_distance < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<rear_2nd_distance>,";
	}
	else noop;

	if (m_pgs_parameter.rear_3rd_distance < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<rear_3rd_distance>,";
	}
	else noop;

	if (m_pgs_parameter.unit_sample_width_mm < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<unit_sample_width_mm>,";
	}
	else noop;

	if (m_pgs_parameter.unit_sample_length_mm < 0.0f)
	{
		is_error = true;
		error_msg += " pgs_parameter<unit_sample_length_mm>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative float number!";
	}
	else noop;

	// check range
	if (m_pgs_parameter.unit_sample_length_mm > m_pgs_parameter.guide_max_distance)
	{
		is_error = true;
		error_msg += " pgs_parameter<unit_sample_length_mm> must be smaller than pgs_parameter<guide_max_distance>!";
	}
	else noop;

	// check divisor
	if (!CHECK_IS_INTEGER(m_pgs_parameter.guide_max_distance / m_pgs_parameter.unit_sample_length_mm))
	{
		is_error = true;
		error_msg += " pgs_parameter<unit_sample_length_mm> must be a divisor of pgs_parameter<guide_max_distance>!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_dgs_parameters()
{
	bool is_error = false;
	string error_msg = "";

	// check valid number
	if (m_dgs_parameter.monitoring_distance < 0.0f)
	{
		is_error = true;
		error_msg += " dgs_parameter<monitoring_distance>,";
	}
	else noop;

	if (m_dgs_parameter.unit_square_length < 0.0f)
	{
		is_error = true;
		error_msg += " dgs_parameter<unit_square_length>,";
	}
	else noop;

	if (m_dgs_parameter.layout0_line_thickness < 0.0f)
	{
		is_error = true;
		error_msg += " dgs_parameter<layout0_line_thickness>,";
	}
	else noop;

	if (m_dgs_parameter.layout1_line_thickness < 0.0f)
	{
		is_error = true;
		error_msg += " dgs_parameter<layout1_line_thickness>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative float number!";
	}
	else noop;

	// check range
	if (m_dgs_parameter.unit_square_length > m_dgs_parameter.monitoring_distance)
	{
		is_error = true;
		error_msg += " dgs_parameter<unit_square_length> must be smaller than dgs_parameter<monitoring_distance>!";
	}
	else noop;

	// check divisor
	if (!CHECK_IS_INTEGER(m_dgs_parameter.monitoring_distance / m_dgs_parameter.unit_square_length))
	{
		is_error = true;
		error_msg += " dgs_parameter<unit_square_length> must be a divisor of dgs_parameter<monitoring_distance>!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_od_parameters()
{
	bool is_error = false;
	string error_msg = "";

	// check valid float number
	if (m_od_parameter.warning_min_distance < 0.0f)
	{
		is_error = true;
		error_msg += " od_parameter<warning_min_distance>,";
	}
	else noop;

	if (m_od_parameter.warning_max_distance < 0.0f)
	{
		is_error = true;
		error_msg += " od_parameter<warning_max_distance>,";
	}
	else noop;

	if (m_od_parameter.circle_target_radius < 0.0f)
	{
		is_error = true;
		error_msg += " od_parameter<circle_target_radius>,";
	}
	else noop;

	if (m_od_parameter.target_gradient_time_msec < 0.0f)
	{
		is_error = true;
		error_msg += " od_parameter<target_gradient_time_msec>,";
	}
	else noop;

	if (m_od_parameter.duplicate_object_iou_threshold < 0.0f)
	{
		is_error = true;
		error_msg += " od_parameter<duplicate_object_iou_threshold>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative float number!";
	}
	else noop;

	// check valid integer number
	is_error = false;
	if (m_od_parameter.coast_cycles_threshold < 0.0f || !CHECK_IS_INTEGER(m_od_parameter.coast_cycles_threshold))
	{
		is_error = true;
		error_msg += " od_parameter<coast_cycles_threshold>,";
	}
	else noop;

	if (m_od_parameter.min_hit_streak < 0.0f || !CHECK_IS_INTEGER(m_od_parameter.min_hit_streak))
	{
		is_error = true;
		error_msg += " od_parameter<min_hit_streak>,";
	}
	else noop;

	if (is_error)
	{
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative integer number!";
	}
	else noop;

	return error_msg;
}

string sanXML::check_range_mobs_parameters()
{
	bool is_error = false;
	string error_msg = "";

	// check valid positive values
	bool is_positive_error = false;
	if (m_mobs_parameter.mois_speed_max <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<mois_speed_max>,";
	}
	else noop;

	if (m_mobs_parameter.bsis_speed_max <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<bsis_speed_max>,";
	}
	else noop;

	if (m_mobs_parameter.reverse_speed_max <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<reverse_speed_max>,";
	}
	else noop;

	if (m_mobs_parameter.mois_ttc_warning <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<mois_ttc_warning>,";
	}
	else noop;

	if (m_mobs_parameter.mois_ttc_monitoring <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<mois_ttc_monitoring>,";
	}
	else noop;

	if (m_mobs_parameter.bsis_ttc_warning <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<bsis_ttc_warning>,";
	}
	else noop;

	if (m_mobs_parameter.bsis_ttc_monitoring <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<bsis_ttc_monitoring>,";
	}
	else noop;

	if (m_mobs_parameter.reverse_ttc_warning <= 0.0f)
	{
		is_positive_error = true;
		error_msg += " mobs_parameter<reverse_ttc_warning>,";
	}
	else noop;

	if (is_positive_error)
	{
		is_error = true;

		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be positive float number!";
	}
	else noop;

	// check valid non-negative values
	bool is_non_negative_error = false;
	if (m_mobs_parameter.mois_speed_min < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<mois_speed_min>,";
	}
	else noop;

	if (m_mobs_parameter.bsis_speed_min < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<bsis_speed_min>,";
	}
	else noop;

	if (m_mobs_parameter.reverse_speed_min < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<reverse_speed_min>,";
	}
	else noop;

	if (m_mobs_parameter.mois_dist_warning_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<mois_dist_warning_head>,";
	}
	else noop;

	if (m_mobs_parameter.mois_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<mois_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.mois_dist_monitoring_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<mois_dist_monitoring_head>,";
	}
	else noop;

	if (m_mobs_parameter.mois_dist_monitoring_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<mois_dist_monitoring_side>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_warning_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_warning_head>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_warning_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_warning_tail>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_monitoring_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_monitoring_head>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_monitoring_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_monitoring_side>,";
	}
	else noop;

	if (m_mobs_parameter.right_bsis_dist_monitoring_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_bsis_dist_monitoring_tail>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_warning_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_warning_head>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_warning_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_warning_tail>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_monitoring_head < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_monitoring_head>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_monitoring_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_monitoring_side>,";
	}
	else noop;

	if (m_mobs_parameter.left_bsis_dist_monitoring_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_bsis_dist_monitoring_tail>,";
	}
	else noop;

	if (m_mobs_parameter.right_rear_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_rear_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.right_rear_dist_warning_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<right_rear_dist_warning_tail>,";
	}
	else noop;

	if (m_mobs_parameter.left_rear_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_rear_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.left_rear_dist_warning_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<left_rear_dist_warning_tail>,";
	}
	else noop;

	if (m_mobs_parameter.center_rear_dist_warning_side < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<center_rear_dist_warning_side>,";
	}
	else noop;

	if (m_mobs_parameter.center_rear_dist_warning_near_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<center_rear_dist_warning_near_tail>,";
	}
	else noop;

	if (m_mobs_parameter.center_rear_dist_warning_far_tail < 0.0f)
	{
		is_non_negative_error = true;
		error_msg += " mobs_parameter<center_rear_dist_warning_far_tail>,";
	}
	else noop;

	if (is_non_negative_error)
	{
		is_error = true;
		// finish and check if error is existed
		if (error_msg.length())
			error_msg.pop_back(); // remove last character ","

		error_msg += " must be non-negative float number!";
	}
	else noop;

	// check valid range of min and max velocity, and ttc
	bool is_range_error = false;
	if (m_mobs_parameter.mois_speed_min > m_mobs_parameter.mois_speed_max)
	{
		is_range_error = true;
		error_msg += " mobs_parameter<mois_speed_min> must be no larger than mobs_parameter<mois_speed_max>!";
	}
	else noop;

	if (m_mobs_parameter.bsis_speed_min > m_mobs_parameter.bsis_speed_max)
	{
		is_range_error = true;
		error_msg += " mobs_parameter<bsis_speed_min> must be no larger than mobs_parameter<bsis_speed_max>!";
	}
	else noop;

	if (m_mobs_parameter.reverse_speed_min > m_mobs_parameter.reverse_speed_max)
	{
		is_range_error = true;
		error_msg += " mobs_parameter<reverse_speed_min> must be no larger than mobs_parameter<reverse_speed_max>!";
	}
	else noop;

	if (m_mobs_parameter.mois_ttc_warning > m_mobs_parameter.mois_ttc_monitoring)
	{
		is_range_error = true;
		error_msg += " mobs_parameter<mois_ttc_warning> must be no larger than mobs_parameter<mois_ttc_monitoring>!";
	}
	else noop;

	if (m_mobs_parameter.bsis_ttc_warning > m_mobs_parameter.bsis_ttc_monitoring)
	{
		is_range_error = true;
		error_msg += " mobs_parameter<bsis_ttc_warning> must be no larger than mobs_parameter<bsis_ttc_monitoring>!";
	}
	else noop;

	if(is_range_error)
	{
		is_error = true;
	}
	else noop;

	if(is_error)
	{
		
	}
	else noop;

	return error_msg;
}
