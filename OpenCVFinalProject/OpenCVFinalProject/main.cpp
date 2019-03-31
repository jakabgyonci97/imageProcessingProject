#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <stdlib.h>
using namespace std;
using namespace cv;

vector<int> horizontalProjection(Mat img,const uchar pixelValue, const int amplifier) {
	vector<int> horProj(img.rows, 0);

	for (int i = 0; i < img.rows; i++) {
		for (int j = 0; j < img.cols; j++) {
			if (img.at<uchar>(i, j) == pixelValue)horProj[i]++;
		}
		horProj[i] *= amplifier;
	}
	return horProj;
}
vector<int> verticalProjection(Mat img, const uchar pixelValue, const int amplifier) {
	vector<int> verProj(img.cols, 0);

	for (int j = 0; j < img.cols; j++) {
		for (int i = 0; i < img.rows; i++) {
			if (img.at<uchar>(i, j) == pixelValue)verProj[j]++;
		}
		verProj[j] *= amplifier;
	}
	return verProj;
}
Mat viewProjection(vector<int> projection, const uchar type,const int hight,const int width) {
	Mat img(hight, width, CV_8UC1,255);
	if (!type) {
		for (int i = 0; i < projection.size(); i++) {
			Point p1(0, i);
			Point p2(projection[i], i);
			line(img, p1, p2, 0, 1);
		}
	}
	else {
		for (int i = 0; i < projection.size(); i++) {
			Point p1(i,0);
			Point p2(i,projection[i]);
			line(img, p1, p2, 0, 1);
		}
	}
	return img;
}
Mat threshold(Mat img, const uchar TH) {
	Mat binary(img.rows, img.cols, CV_8UC1);

	for (int i = 0; i < img.rows; i++) {
		for (int j = 0; j < img.cols; j++) {
			if (img.at<uchar>(i, j) <= TH) binary.at<uchar>(i, j) = 0;
			else binary.at<uchar>(i, j) = 255;
		}
	}
	return binary;
}

vector<int> indentifyLinePositions(Mat img,const uchar ObjectColor,const uchar BackGroundColor) {
	vector<int> lineRows;

	for (int i = 0; i < img.rows; i++) {
		if (img.at<uchar>(i, img.cols / 2) == ObjectColor)
			lineRows.push_back(i);
	}
	if (lineRows.size() == 5) return lineRows;
	vector<int> newLineRows;
	int countLines;
	for (int i = 0,j; i < lineRows.size(); i++) {
		if (lineRows[i] + 1 != lineRows[i + 1]) newLineRows.push_back(lineRows[i]);
		else {
			countLines = 0;
			for (j = i + 1; j < lineRows.size() && lineRows[i] + 1 == lineRows[i + 1];) {
				j++;
				countLines += lineRows[j];
			}
			countLines /= (j - i);
			newLineRows.push_back(countLines);
		}
	}
	return newLineRows;
}
vector<Mat> _2MusicalSignsSegmentation(Mat img) {
	vector<Mat> signs;
	return signs;
}
vector<int> removeHorizontalLines(Mat src, Mat dest) {
	Mat horizontal = src.clone();
	Mat vertical = src.clone();

	int horizontalsize = horizontal.cols / 30;
	Mat horizontalStructure = getStructuringElement(MORPH_RECT, Size(horizontalsize, 1));
	erode(horizontal, horizontal, horizontalStructure, Point(-1, -1));
	dilate(horizontal, horizontal, horizontalStructure, Point(-1, -1));

	vector<int> lineRows = indentifyLinePositions(horizontal, 255, 0);
	imshow("horizontal", horizontal);

	int verticalsize = vertical.rows / 15;
	Mat verticalStructure = getStructuringElement(MORPH_RECT, Size(1, verticalsize));
	erode(vertical, vertical, verticalStructure, Point(-1, -1));
	dilate(vertical, vertical, verticalStructure, Point(-1, -1));
	imshow("vertical", vertical);

	bitwise_not(vertical, vertical);

	Mat edges;
	adaptiveThreshold(vertical, edges, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 3, -2);

	Mat kernel = Mat::ones(2, 2, CV_8UC1);
	dilate(edges, edges, kernel);

	vertical.copyTo(dest);

	blur(dest, dest, Size(2, 2));

	dest.copyTo(vertical, edges);

	imshow("smooth", dest);
	return lineRows;
}
vector<Mat> _2GroupsSegmentation(Mat img, const int TH_ERROR, vector<int> lineRows) {
	Mat bw;
	adaptiveThreshold(~img, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, -2);
	Mat imgRemLine;

	lineRows = removeHorizontalLines(bw, imgRemLine);
	vector<int> verticalProj = verticalProjection(imgRemLine, 0, 1);

	vector<Rect> whiteSpaces;

	int start = 0, width = 0;
	for (int i = 0; i < verticalProj.size(); i++) {
		if (verticalProj[i] < 1) {
			if (width == 0)
				start = i;
			width++;
		}
		else{
			if (width >= TH_ERROR) {
				Rect nr(start, 0, width, imgRemLine.rows);
				//rectangle(imgRemLine, nr, 128, 1);
				whiteSpaces.push_back(nr);
			}
			width = 0;
		}
		if (i == verticalProj.size() - 1) {
			if (width >= TH_ERROR && width <= (img.cols/4)) {
				Rect nr(start, 0, width, imgRemLine.rows);
				//rectangle(imgRemLine, nr, 128, 1);
				whiteSpaces.push_back(nr);
			}
		}
	}
	vector<Mat> groups;
	for (int i = 0; i < whiteSpaces.size() - 1; i++) {
		int newX = whiteSpaces[i].x + whiteSpaces[i].width-3;
		int newY = whiteSpaces[i].y;
		int newHeight = whiteSpaces[i].height;
		int newWidth = whiteSpaces[i+1].x - newX;
		
		if (newWidth >=TH_ERROR) {
			Rect noteRect(newX, newY, newWidth, newHeight);
			Mat roi = img(noteRect);
			groups.push_back(roi);
		}
	}
	return groups;
}
vector<Mat> _2LinesSegmentation(Mat img,const int TH_ERROR) {
	vector<int> horizontalProj = horizontalProjection(img, 0, 1);
	Mat horProj = viewProjection(horizontalProj, 0, img.rows, img.cols);
	//imshow("Horizontal projection", horProj);

	vector<Mat> lines;
	int start = 0, height = 0;
	for (int i = 0; i < horizontalProj.size(); i++) {
		if (horizontalProj[i] >= 1) {
			if (height == 0)
				start = i;
			height++;
		}
		else if(horizontalProj[i] < 1 || i >= horizontalProj.size() - 1){
			if (height > 0) {
				if (height >= TH_ERROR) {
					Rect nr(0, start, img.cols, height);
					//rectangle(img, nr, 128, 2);
					Mat roi = img(nr);
					lines.push_back(roi);
				}
				height = 0;
			}
		}
	}
	return lines;
}
int main() {
	Mat img = imread("musical_sheet/sheet1.jpg", 0);
	imshow("Original image", img);
	Mat binary = threshold(img, 200);
	vector<Mat> lines = _2LinesSegmentation(binary,20);
	
	char lineName[20] = "ready/line0.bmp";
	char groupName[30] = "ready/line0_group0.bmp";
	vector<Mat> groups;
	vector<vector<int>> imageLines(lines.size());

	for (int i = 0; i < lines.size(); i++) {
		
		groups = _2GroupsSegmentation(lines[i], 6,imageLines[i]);
		lineName[10] = (char)(48 + i);
		imwrite(lineName, lines[i]);

		groupName[10] = (char)(48 + i);
		for (int j = 0; j < groups.size(); j++) {
			groupName[17] = (char)(48 + j);
			imwrite(groupName, groups[j]);
		}
		groups.clear();
	}
	cout << "end" << endl;
	waitKey(0);
	return 0;
}