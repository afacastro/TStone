#include "findCorner.h"
#include <algorithm>
#include <math.h>

struct Corner_t findCorner(cv::Mat img, double tau, bool refine)
{
	if (img.channels() == 3)
		cv::cvtColor(img, img, CV_BGR2GRAY);
	cv::Mat du = (cv::Mat_<float>(3, 3) << -1, 0, 1, -1, 0, 1, -1, 0, 1);
	cv::Mat dv = du.t();
	cv::Mat img_dv, img_du;
	cv::Mat img_angle = cv::Mat::zeros(img.size(), CV_32FC1);
	cv::Mat img_weight = cv::Mat::zeros(img.size(), CV_32FC1);
	cv::Mat img_float;
	img.convertTo(img_float, CV_32F);
	cv::filter2D(img_float, img_du, img_float.depth(), du, cvPoint(-1, -1));
	cv::filter2D(img_float, img_dv, img_float.depth(), dv, cvPoint(-1, -1));
	for (int i = 0; i < img.rows; i++)
	{
		for (int j = 0; j < img.cols; j++)
		{
			img_angle.at<float>(i, j) = float(atan2(img_dv.at<float>(i, j), img_du.at<float>(i, j)));
			if (img_angle.at<float>(i, j) < 0) img_angle.at<float>(i, j) += M_PI;
			if (img_angle.at<float>(i, j) > M_PI) img_angle.at<float>(i, j) -= M_PI;
			img_weight.at<float>(i, j) = sqrt(img_dv.at<float>(i, j) * img_dv.at<float>(i, j) + img_du.at<float>(i, j) * img_du.at<float>(i, j));
		}
	}
	double minVal, maxVal;
	int minIdx[2] = {}, maxIdx[2] = {};
	cv::minMaxIdx(img_float, &minVal, &maxVal, minIdx, maxIdx);
	for (int i = 0; i < img_float.rows; i++)
	{
		for (int j = 0; j < img_float.cols; j++)
		{
			img_float.at<float>(i, j) = (img_float.at<float>(i, j) - float(minVal)) / (float(maxVal - minVal));
		}
	}
	std::cout << "Filtering ..." << std::endl;
	cv::Mat template_props = (cv::Mat_<float>(6, 3) << 0, M_PI/2, 4, -M_PI/4, M_PI/4, 4, 0, M_PI/2, 8, -M_PI/4, M_PI/4, 8, 0, M_PI/2, 12, -M_PI/4, M_PI/4, 12);
	cv::Mat img_corner = cv::Mat::zeros(img.size(), CV_32FC1);
	for (int i = 0; i < template_props.rows; i++)
	{
		struct Template_t corner_template = createCorrelationPatch(template_props.at<float>(i, 0), template_props.at<float>(i, 1), int(template_props.at<float>(i, 2)));
		cv::Mat img_corners_a1, img_corners_a2, img_corners_b1, img_corners_b2, img_corners_mu;
		cv::filter2D(img_float, img_corners_a1, img_float.depth(), corner_template.a1, cvPoint(-1, -1));
		cv::filter2D(img_float, img_corners_a2, img_float.depth(), corner_template.a2, cvPoint(-1, -1));
		cv::filter2D(img_float, img_corners_b1, img_float.depth(), corner_template.b1, cvPoint(-1, -1));
		cv::filter2D(img_float, img_corners_b2, img_float.depth(), corner_template.b2, cvPoint(-1, -1));
		for (int j = 0; j < img_float.rows; j++)
		{
			for (int k = 0; k < img_float.cols; k++)
			{
				float a1 = img_corners_a1.at<float>(j, k);
				float a2 = img_corners_a2.at<float>(j, k);
				float b1 = img_corners_b1.at<float>(j, k);
				float b2 = img_corners_b2.at<float>(j, k);
				float mu = (a1 + a2 + b1 + b2) / 4;
				float img_corners_a = std::min(a1 - mu, a2 - mu);
				float img_corners_b = std::min(mu - b1, mu - b2);
				float img_corners_1 = std::min(img_corners_a, img_corners_b);

				img_corners_a = std::min(mu - a1, mu - a2);
				img_corners_b = std::min(b1 - mu, b2 - mu);
				float img_corners_2 = std::min(img_corners_a, img_corners_b);

				img_corner.at<float>(j, k) = std::max(img_corner.at<float>(j, k), img_corners_1);
				img_corner.at<float>(j, k) = std::max(img_corner.at<float>(j, k), img_corners_2);
			}
		}
	}
	struct Corner_t corners;
	corners.p = nonMaximumSuppression(img_corner, 3, 0.025, 0);
	std::cout << "Refining ..." << std::endl;
	corners = refineCorners(img_du, img_dv, img_angle, img_weight, corners, 10);
	auto iterator_v1 = corners.v1.begin();
	auto iterator_v2 = corners.v2.begin();
	auto iterator_p = corners.p.begin();
	std::vector<float>::iterator iterator_score = corners.score.begin();
	while (iterator_v1 != corners.v1.end())
	{
		if (iterator_v1->x == 0 && iterator_v1->y == 0)
		{
			corners.v1.erase(iterator_v1);
			corners.v2.erase(iterator_v2);
			corners.p.erase(iterator_p);
			corners.score.erase(iterator_score);
			iterator_v1--;
			iterator_v2--;
			iterator_p--;
			iterator_score--;
		}
		iterator_v1++;
		iterator_v2++;
		iterator_p++;
		iterator_score++;
	}
	std::cout << "Scoring ..." << std::endl;
	int radius[3] = { 4, 8, 12 };
	corners = scoreCorners(img_float, img_weight, corners, radius);
	iterator_v1 = corners.v1.begin();
	iterator_v2 = corners.v2.begin();
	iterator_p = corners.p.begin();
	iterator_score = corners.score.begin();
	while (iterator_v1 != corners.v1.end())
	{
		if (*iterator_score < tau)
		{
			corners.v1.erase(iterator_v1);
			corners.v2.erase(iterator_v2);
			corners.p.erase(iterator_p);
			corners.score.erase(iterator_score);
			iterator_v1--;
			iterator_v2--;
			iterator_p--;
			iterator_score--;
		}
		iterator_v1++;
		iterator_v2++;
		iterator_p++;
		iterator_score++;
	}
	for (int i = 0; i < corners.v1.size(); i++)
	{
		if (corners.v1[i].x + corners.v1[i].y < 0)
		{
			corners.v1[i].x *= -1;
			corners.v1[i].y *= -1;
		}
		cv::Point2f corners_n1;
		corners_n1.x = corners.v1[i].y;
		corners_n1.y = -corners.v1[i].x;
		int flip = -sign(corners_n1.x*corners.v2[i].x+corners_n1.y*corners.v2[i].y);
		corners.v2[i].x = corners.v2[i].x * flip;
		corners.v2[i].y = corners.v2[i].y * flip;
	}
	return corners;
}

int sign(double x)
{
	if (x < 0)
		return -1;
	else if (x > 0)
		return 1;
	else
		return 0;
}

struct Template_t createCorrelationPatch(float angle_1, float angle_2, int radius)
{
	struct Template_t result;
	int width = radius * 2 + 1;
	int height = radius * 2 + 1;

	result.a1 = cv::Mat::zeros(cv::Size(width, height), CV_32F);
	result.a2 = cv::Mat::zeros(cv::Size(width, height), CV_32F);
	result.b1 = cv::Mat::zeros(cv::Size(width, height), CV_32F);
	result.b2 = cv::Mat::zeros(cv::Size(width, height), CV_32F);

	int mu = radius;
	int mv = radius;

	for (int u = 0; u < width; u++)
	{
		for (int v = 0; v < height; v++)
		{
			int du = u - mu;
			int dv = v - mv;
			double dist = sqrt(du*du + dv*dv);
			float s1 = -du*sin(angle_1) + dv*cos(angle_1);
			float s2 = -du*sin(angle_2) + dv*cos(angle_2);
			if (s1 <= -0.1 && s2 <= -0.1)
				result.a1.at<float>(v, u) = normpdf(dist, 0, radius / 2);
			else if( s1 >= 0.1 && s2 >= 0.1)
				result.a2.at<float>(v, u) = normpdf(dist, 0, radius / 2);
			else if( s1 <= -0.1 && s2 >= 0.1)
				result.b1.at<float>(v, u) = normpdf(dist, 0, radius / 2);
			else if( s1 >= 0.1 && s2 <= -0.1)
				result.b2.at<float>(v, u) = normpdf(dist, 0, radius / 2);
		}
	}
	cv::normalize(result.a1, result.a1, 1, 0, cv::NORM_L1);
	cv::normalize(result.a2, result.a2, 1, 0, cv::NORM_L1);
	cv::normalize(result.b1, result.b1, 1, 0, cv::NORM_L1);
	cv::normalize(result.b2, result.b2, 1, 0, cv::NORM_L1);
	return result;
}

float normpdf(double x, int mu, int sigma)
{
	return float(exp(-(x - mu)*(x - mu)/2/sigma/sigma) / sqrt(2 * M_PI) / sigma);
}

std::vector<cv::Point2f> nonMaximumSuppression(cv::Mat img, int n, float tau, int margin)
{
	std::vector<cv::Point2f> results;
	int width = img.cols;
	int height = img.rows;
	for (int i = n + margin; i < width - n - margin; i += n + 1)
	{
		for (int j = n + margin; j < height - n - margin; j += n + 1)
		{
			int maxi = i;
			int maxj = j;
			float maxval = img.at<float>(j, i);
			for (int i2 = i; i2 <= i + n; i2++)
			{
				for (int j2 = j; j2 <= j + n; j2++)
				{
					float currval = img.at<float>(j2, i2);
					if (currval > maxval)
					{
						maxi = i2;
						maxj = j2;
						maxval = currval;
					}
				}
			}
			
			bool failed = false;
			for (int i2 = maxi - n; i2 < std::min(maxi + n, width - margin); i2++)
			{
				for (int j2 = maxj - n; j2 < std::min(maxj + n, height - margin); j2++)
				{
					float currval = img.at<float>(j2, i2);
					if (currval > maxval && (i2<i || i2>i + n || j2<j || j2>j + n))
					{
						failed = true;
						break;
					}
				}
				if (failed)
					break;
			}
			if (maxval >= tau && failed == false)
			{
				cv::Point2f point = cv::Point2f(float(maxi), float(maxj));
				results.push_back(point);
			}
		}
	}
	return results;
}

struct Corner_t refineCorners(cv::Mat img_du, cv::Mat img_dv, cv::Mat img_angle, cv::Mat img_weight, struct Corner_t corners, int r)
{
	int height = img_du.rows;
	int width = img_du.cols;
	corners.v1.resize(corners.p.size());
	corners.v2.resize(corners.p.size());
	corners.score.resize(corners.p.size());
	for (int i = 0; i < corners.p.size(); i++)
	{
		int cu = corners.p[i].x;
		int cv = corners.p[i].y;
		int x1 = std::min(cv + r, height - 1);
		int x2 = std::max(cv - r, 0);
		int x3 = std::min(cu + r, width - 1);
		int x4 = std::max(cu - r, 0);
		cv::Mat img_angle_sub = cv::Mat::zeros(cv::Size(x3 - x4 + 1, x1 - x2 + 1), CV_32F);
		cv::Mat img_weight_sub = cv::Mat::zeros(cv::Size(x3 - x4 + 1, x1 - x2 + 1), CV_32F);
		for (int j = 0; j <= x1 - x2; j++)
		{
			for (int k = 0; k <= x3 - x4; k++)
			{
				img_angle_sub.at<float>(j, k) = img_angle.at<float>(j + x2, k + x4);
				img_weight_sub.at<float>(j, k) = img_weight.at<float>(j + x2, k + x4);
			}
		}
		cv::Point2f v1, v2;
		edgeOrientations(img_angle_sub, img_weight_sub, &v1, &v2);
		corners.v1[i] = v1;
		corners.v2[i] = v2;
		if ((v1.x == 0 && v1.y == 0) || (v2.x == 0 && v2.y == 0))
			continue;
		// corner orientation refinement
		cv::Mat A1 = cv::Mat::zeros(cv::Size(2, 2), CV_32F);
		cv::Mat A2 = cv::Mat::zeros(cv::Size(2, 2), CV_32F);
		for (int u = x4; u <= x3; u++)
		{
			for (int v = x2; v <= x1; v++)
			{
				float o[2] = { img_du.at<float>(v, u), img_dv.at<float>(v, u) };
				float o_norm = sqrt(o[0]*o[0]+o[1]*o[1]);
				if (o_norm < 0.1)
					continue;
				o[0] = o[0] / o_norm;
				o[1] = o[1] / o_norm;
				float du = img_du.at<float>(v, u);
				float dv = img_dv.at<float>(v, u);
				if (fabs(o[0] * v1.x + o[1] * v1.y) < 0.25)
				{
					A1.at<float>(0, 0) += du * du;
					A1.at<float>(0, 1) += du * dv;
					A1.at<float>(1, 0) += dv * du;
					A1.at<float>(1, 1) += dv * dv;
				}
				if (fabs(o[0] * v2.x + o[1] * v2.y) < 0.25)
				{
					A2.at<float>(0, 0) += du * du;
					A2.at<float>(0, 1) += du * dv;
					A2.at<float>(1, 0) += dv * du;
					A2.at<float>(1, 1) += dv * dv;
				}
			}
		}
		cv::Mat V, D;
		cv::eigen(A1, D, V);
		v1.x = V.at<float>(1, 0);
		v1.y = V.at<float>(1, 1);
		corners.v1[i] = v1;
		cv::eigen(A2, D, V);
		v2.x = V.at<float>(1, 0);
		v2.y = V.at<float>(1, 1);
		corners.v2[i] = v2;
		// corner location refinement
		cv::Mat G = cv::Mat::zeros(cv::Size(2, 2), CV_32F);
		cv::Mat b = cv::Mat::zeros(cv::Size(1, 2), CV_32F);
		for (int u = x4; u <= x3; u++)
		{
			for (int v = x2; v <= x1; v++)
			{
				float o[2] = { img_du.at<float>(v, u), img_dv.at<float>(v, u) };
				float o_norm = sqrt(o[0] * o[0] + o[1] * o[1]);
				if (o_norm < 0.1)
					continue;
				o[0] = o[0] / o_norm;
				o[1] = o[1] / o_norm;
				if (u != cu || v != cv)
				{
					float du = u - cu;
					float dv = v - cv;
					float du_ = du - (du*v1.x + dv*v1.y)*v1.x;
					float dv_ = dv - (du*v1.x + dv*v1.y)*v1.y;
					float d1 = sqrt(du_*du_ + dv_*dv_);
					du_ = du - (du*v2.x + dv*v2.y)*v2.x;
					dv_ = dv - (du*v2.x + dv*v2.y)*v2.y;
					float d2 = sqrt(du_*du_ + dv_*dv_);
					if ((d1 < 3 && fabs(o[0] * v1.x + o[1] * v1.y) < 0.25) ||
						(d2 < 3 && fabs(o[0] * v2.x + o[1] * v2.y) < 0.25))
					{
						du = img_du.at<float>(v, u);
						dv = img_dv.at<float>(v, u);
						cv::Mat H = (cv::Mat_<float>(2, 2) << du*du, du*dv, du*dv, dv*dv);
						G = G + H;
						cv::Mat uv = (cv::Mat_<float>(2, 1) << u, v);
						b = b + H*uv;
					}
				}
			}
		}
		float G_r = G.at<float>(0, 0)*G.at<float>(1, 1) - G.at<float>(0, 1)*G.at<float>(1, 0);
		if (G_r != 0)
		{
			cv::Point2f corner_pos_old = corners.p[i];
			cv::invert(G, G);
			cv::Mat pos = G*b;
			corners.p[i].x = pos.at<float>(0,0);
			corners.p[i].y = pos.at<float>(1,0);
			float dist = sqrt((corners.p[i].x - corner_pos_old.x)*(corners.p[i].x - corner_pos_old.x)+
				(corners.p[i].y - corner_pos_old.y)*(corners.p[i].y - corner_pos_old.y));
			if (dist >= 4)
			{
				corners.p[i] = cv::Point2f(0,0);
				corners.v1[i] = cv::Point2f(0, 0);
			}
		}
		else
		{
			corners.p[i] = cv::Point2f(0, 0);
			corners.v1[i] = cv::Point2f(0, 0);
		}
	}
	return corners;
}

void edgeOrientations(cv::Mat img_angle, cv::Mat img_weight, cv::Point2f* v1, cv::Point2f* v2)
{
	int bin_num = 32;
	cv::Mat angle_hist = cv::Mat::zeros(cv::Size(bin_num, 1), CV_32F);
	for (int i = 0; i < img_angle.rows; i++)
	{
		for (int j = 0; j < img_angle.cols; j++)
		{
			img_angle.at<float>(i, j) += M_PI / 2;
			if (img_angle.at<float>(i, j) > M_PI) img_angle.at<float>(i, j) -= M_PI;
			int bin = std::max(std::min(int(floor(img_angle.at<float>(i, j) / (M_PI / bin_num))), bin_num - 1), 0);
			angle_hist.at<float>(0, bin) += img_weight.at<float>(i, j);
		}
	}
	std::vector<cv::Point3f> modes = findModesMeanShift(angle_hist, 1);
	if (modes.size() <= 1) return;
	for (int i = 0; i < modes.size(); i++)
	{
		modes[i].z = modes[i].x*M_PI / bin_num;
	}
	float delta_angle = 0;
	if (modes[0].z > modes[1].z)
	{
		*v1 = cv::Point2f(cos(modes[1].z), sin(modes[1].z));
		*v2 = cv::Point2f(cos(modes[0].z), sin(modes[0].z));
		delta_angle = std::min(modes[0].z - modes[1].z, modes[1].z + M_PI - modes[0].z);
	}
	else
	{
		*v1 = cv::Point2f(cos(modes[0].z), sin(modes[0].z));
		*v2 = cv::Point2f(cos(modes[1].z), sin(modes[1].z));
		delta_angle = std::min(modes[1].z - modes[0].z, modes[0].z + M_PI - modes[1].z);
	}
	if (delta_angle <= 0.3)
	{
		*v1 = cv::Point2f(0, 0);
		*v2 = cv::Point2f(0, 0);
	}
}

bool comp(const cv::Point3f &a, const cv::Point3f &b)
{
	return a.y > b.y;
}

std::vector<cv::Point3f> findModesMeanShift(cv::Mat hist, int sigma)
{
	std::vector<cv::Point3f> modes;
	cv::Mat hist_smoothed = hist.clone();
	for (int i = 0; i < hist.cols; i++)
	{
		float sum = 0;
		for (int j = -round(2 * sigma); j <= round(2 * sigma); j++)
		{
			int idx = fmod(i + j, hist.cols);
			sum += hist.at<float>(0, idx) * normpdf(j, 0, sigma);
		}
		hist_smoothed.at<float>(0, i) = sum;
		
	}
	bool failed = true;
	for (int i = 1; i < hist_smoothed.cols; i++)
	{
		if (fabs(hist_smoothed.at<float>(0, i) - hist_smoothed.at<float>(0, 0)) > 1e-5)
		{
			failed = false;
			break;
		}
	}
	if (failed == true)
		return modes;
	
	for (int i = 0; i < hist_smoothed.cols; i++)
	{
		int j = i;
		while (1)
		{
			float h0 = hist_smoothed.at<float>(0, j);
			int j1 = fmod(j + 1, hist_smoothed.cols);
			int j2 = fmod(j - 1, hist_smoothed.cols);
			float h1 = hist_smoothed.at<float>(0, j1);
			float h2 = hist_smoothed.at<float>(0, j2);
			if (h1 >= h0 && h1 >= h2)
				j = j1;
			else if (h2 > h0 && h2 > h1)
				j = j2;
			else
				break;
		}
		cv::Point3f mode = cv::Point3f(j, hist_smoothed.at<float>(0, j), 0);
		if (modes.size() == 0)
			modes.push_back(mode);
		else
		{
			bool find_same = false;
			for (int k = 0; k < modes.size(); k++)
			{
				if (modes[k].x == j)
				{
					find_same = true;
					break;
				}
			}
			if (find_same == false)
				modes.push_back(mode);
		}
	}
	sort(modes.begin(), modes.end(), comp);
	return modes;
}

struct Corner_t scoreCorners(cv::Mat img, cv::Mat img_weight, struct Corner_t corners, int* radius)
{
	int width = img.cols;
	int height = img.rows;
	float score[3] = { 0 };
	for (int i = 0; i < corners.p.size(); i++)
	{
		int u = round(corners.p[i].x);
		int v = round(corners.p[i].y);
		for (int j = 0; j < 3; j++)
		{
			score[j] = 0;
			if (u >= radius[j] && u < width - radius[j] && v >= radius[j] && v < height - radius[j])
			{
				int x1 = v - radius[j];
				int x2 = v + radius[j];
				int x3 = u - radius[j];
				int x4 = u + radius[j];
				cv::Mat img_sub = cv::Mat::zeros(cv::Size(x4 - x3 + 1, x2 - x1 + 1), CV_32F);
				cv::Mat img_weight_sub = cv::Mat::zeros(cv::Size(x4 - x3 + 1, x2 - x1 + 1), CV_32F);
				for (int x = 0; x <= x4 - x3; x++)
				{
					for (int y = 0; y <= x2 - x1; y++)
					{
						img_sub.at<float>(y, x) = img.at<float>(y + x1, x + x3);
						img_weight_sub.at<float>(y, x) = img_weight.at<float>(y + x1, x + x3);
					}
				}
				score[j] = cornerCorrelationScore(img_sub, img_weight_sub, corners.v1[i], corners.v2[i]);
			}
		}
		std::sort(score, score + 3);
		corners.score[i] = score[2];
	}
	return corners;
}

float cornerCorrelationScore(cv::Mat img, cv::Mat img_weight, cv::Point2f v1, cv::Point2f v2)
{
	int c[2] = { (img_weight.rows - 1) / 2, (img_weight.rows - 1) / 2 };
	cv::Mat img_filter = cv::Mat::ones(img_weight.size(), CV_32F) * -1;
	for (int x = 0; x < img_weight.cols; x++)
	{
		for (int y = 0; y < img_weight.rows; y++)
		{
			float p1[2], p2[2], p3[2];
			p1[0] = x - c[0];
			p1[1] = y - c[1];
			p2[0] = (p1[0] * v1.x + p1[1] * v1.y)*v1.x;
			p2[1] = (p1[0] * v1.x + p1[1] * v1.y)*v1.y;
			p3[0] = (p1[0] * v2.x + p1[1] * v2.y)*v2.x;
			p3[1] = (p1[0] * v2.x + p1[1] * v2.y)*v2.y;
			p2[0] = p1[0] - p2[0];
			p2[1] = p1[1] - p2[1];
			p3[0] = p1[0] - p3[0];
			p3[1] = p1[1] - p3[1];
			if (sqrt(p2[0] * p2[0] + p2[1] * p2[1]) <= 1.5 ||
				sqrt(p3[0] * p3[0] + p3[1] * p3[1]) <= 1.5)
			{
				img_filter.at<float>(y, x) = 1;
			}
		}
	}
	cv::Mat mat_mean, mat_stddev;
	cv::meanStdDev(img_weight, mat_mean, mat_stddev);
	img_weight = (img_weight - mat_mean.at<double>(0, 0)) / mat_stddev.at<double>(0, 0);
	cv::meanStdDev(img_filter, mat_mean, mat_stddev);
	img_filter = (img_filter - mat_mean.at<double>(0, 0)) / mat_stddev.at<double>(0, 0);
	float sum = 0, score_gradient = 0;
	for (int i = 0; i < img_weight.rows; i++)
	{
		for (int j = 0; j < img_weight.cols; j++)
		{
			sum += img_weight.at<float>(i, j)*img_filter.at<float>(i, j);
		}
	}
	score_gradient = std::max(sum / (img_weight.rows*img_weight.cols - 1), 0.0f);
	struct Template_t Template;
	Template = createCorrelationPatch(atan2(v1.y, v1.x), atan2(v2.y, v2.x), c[0]);
	float a1 = 0, a2 = 0, b1 = 0, b2 = 0;
	for (int i = 0; i < img.rows; i++)
	{
		for (int j = 0; j < img.cols; j++)
		{
			a1 += Template.a1.at<float>(i, j) * img.at<float>(i, j);
			a2 += Template.a2.at<float>(i, j) * img.at<float>(i, j);
			b1 += Template.b1.at<float>(i, j) * img.at<float>(i, j);
			b2 += Template.b2.at<float>(i, j) * img.at<float>(i, j);
		}
	}
	float mu = (a1 + a2 + b1 + b2) / 4;
	float score_a = std::min(a1 - mu, a2 - mu);
	float score_b = std::min(mu - b1, mu - b2);
	float score_1 = std::min(score_a, score_b);
	score_a = std::min(mu - a1, mu - a2);
	score_b = std::min(b1 - mu, b2 - mu);
	float score_2 = std::min(score_a, score_b);
	float score_intensity = std::max(std::max(score_1, score_2), 0.0f);
	return score_gradient*score_intensity;
}