#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <stdlib.h>
#include <string.h>
using namespace std;
using namespace cv;

#define THRESHHOLD_ERROR 1.5
#define LINE_SEG_ERROR 20
#define GROUP_SEG_ERROR 6

//#define SHOW_STEPS

vector<Rect> lineBoundingBoxes;
vector<vector<Rect>> groupBoundingBoxes(20);
vector<vector<Rect>> noteBoundingBoxes(20);

/**automized binarization*/
vector<int> calculateHistogram(Mat img) {


	vector<int> histo(256, 0);

	for (int i = 0; i < img.rows; i++) {
		for (int j = 0; j < img.cols; j++) {
			histo[img.at<uchar>(i, j)] ++;
		}
	}
	return histo;
}
int maxHistoValue(vector<int> histo) {

	int max = 0;
	for (int i = 0; i < histo.size(); i++)
		if (histo[i] >= max) max = histo[i];
	return max;
}
Mat showHistogram(vector<int> histo, int histo_height, int hist_width) {

	int maxHisto = maxHistoValue(histo);

	Mat histoImage(histo_height + 20, hist_width, CV_8UC1, 128);

	float scale = (float)histo_height / maxHisto;
	int baseline = histo_height - 1 + 20;

	for (int i = 0; i < 15; i++) {
		for (int j = 0; j < hist_width; j++) {
			histoImage.at<uchar>(i, j) = j;
		}
	}

	for (int i = 0; i < histo.size(); i++) {
		Point p1 = Point(i, baseline);
		Point p2 = Point(i, baseline - cvRound(histo[i] * scale));
		line(histoImage, p1, p2, 0);
	}

	return  histoImage;
}
uchar minHistogramIntensity(vector<int> histo) {
	for (int i = 0; i < histo.size(); i++) {
		if (histo[i] != 0)
			return i;
	}
	return histo.size() - 1;
}
uchar maxHistogramIntensity(vector<int> histo) {
	for (int i = histo.size() - 1; i >= 0; i--) {
		if (histo[i] != 0)
			return i;
	}
	return 0;
}
uchar findThresholdValue(Mat img, float ERROR) {
	vector<int> histogram = calculateHistogram(img);

	uchar Imax = maxHistogramIntensity(histogram);
	uchar Imin = minHistogramIntensity(histogram);

	float T, Tnew = 128.0;
	float q1, q2, n, sum;
	do {
		T = Tnew;
		sum = 0;
		n = 0;
		for (int i = Imin; i <= T; i++) {
			sum += i * histogram[i];
			n += histogram[i];
		}
		q1 = sum / n;

		sum = 0;
		n = 0;
		for (int i = T + 1; i <= Imax; i++) {
			sum += i * histogram[i];
			n += histogram[i];
		}
		q2 = sum / n;

		Tnew = (q1 + q2) / 2;

	} while (abs(Tnew - T) < ERROR);
	return Tnew;
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


Mat createFinalBoundingBoxes(Mat img) {
	Mat dst(img.rows, img.cols, CV_8UC3);

	for (int i = 0; i < img.rows; i++) {
		for (int j = 0; j < img.cols; j++) {
			dst.at<Vec3b>(i, j) = Vec3b(img.at<uchar>(i,j), img.at<uchar>(i, j), img.at<uchar>(i, j));
		}
	}

	for (int i = 0; i < lineBoundingBoxes.size();i++) {
		Rect current = lineBoundingBoxes[i];
		rectangle(dst, current, Vec3b(0, 255, 255), 2);
	}

	for (int i = 0; i < groupBoundingBoxes.size(); i++) {
		for (int j = 0; j < groupBoundingBoxes[i].size(); j++) {
			Rect current = groupBoundingBoxes[i][j];
			rectangle(dst, current, Vec3b(255, 0, 255), 1);
		}
		
	}
	return dst;
}

vector<Mat> _2MusicalSignsSegmentation(Mat img, vector<int> linePosition) {
	vector<Mat> signs;

	vector<int> verticalProj = verticalProjection(img, 0, 1);
	#ifdef SHOW_STEPS
		Mat vertical = viewProjection(verticalProj, 1, img.rows, img.cols);
		imshow("Original Group", img);
		imshow("Group projection", vertical);
	#endif
	
	//note that between each possible note there is a line that at each point has constant number of black pixels
	//also note that a group starts with a notes round head thingy

	for (int i = 0; i < 10; i++) {
		verticalProj.push_back(0);
	}

	//identify constant segments
	for (int i = 0; i < verticalProj.size(); i++) {
		cout << verticalProj[i] << " ";
	}
	cout << endl;

	return signs;
}
vector<vector<Mat>> analizeAllGroups(vector<vector<Mat>> groups,int groupWidth) {
	vector<vector<Mat>> notes(groups.size());

	for (int i = 0; i < groups.size(); i++) {
		for (int j = 0; j < groups[i].size(); j++) {
			if (groups[i][j].cols <= groupWidth) {
				notes[i].push_back(groups[i][j]);
			}	
			else {
				vector<int> lines;
				vector<Mat> newNotes = _2MusicalSignsSegmentation(groups[i][j],lines);
				for (int k = 0; k < newNotes.size(); k++) {
					notes[i].push_back(newNotes[k]);
				}
			}
		}
	}

	return notes;
}
int findMedianGroupWidth(vector<vector<Mat>> groups) {
	float median = 0;
	int groupNumber = 0;
	for (int i = 0; i < groups.size(); i++) {
		for (int j = 0; j < groups[i].size(); j++) {
			median += groups[i][j].cols;
			groupNumber++;
		}
	}
	return median / groupNumber;
}
vector<int> indentifyLinePositions(Mat img, const uchar ObjectColor, const uchar BackGroundColor) {

	vector<int> lineRows;
	for (int i = 0; i < img.rows; i++) {
		if (img.at<uchar>(i, img.cols / 2) == ObjectColor)
			lineRows.push_back(i);
	}
	if (lineRows.size() == 5) return lineRows;
	
	//reduce duplicates line elements -> lineRows[i] + 1 = lineRows[i]
	lineRows.push_back(-1);

	vector<int> newLineRows;
	int n = lineRows.size();
	n--;
	for (int i = 0; i < n;i++) {
		if ((lineRows[i] + 1) != lineRows[i + 1]) {
			newLineRows.push_back(lineRows[i]);
		}
	}
	return newLineRows;
}
vector<Mat> removeHorizontalLines(Mat src) {
	Mat horizontal = src.clone();
	Mat vertical = src.clone();

	int horizontalsize = horizontal.cols / 30;
	Mat horizontalStructure = getStructuringElement(MORPH_RECT, Size(horizontalsize, 1));
	erode(horizontal, horizontal, horizontalStructure, Point(-1, -1));
	dilate(horizontal, horizontal, horizontalStructure, Point(-1, -1));

	#ifdef SHOW_STEPS
		imshow("horizontal", horizontal);
	#endif

	int verticalsize = vertical.rows / 15;
	Mat verticalStructure = getStructuringElement(MORPH_RECT, Size(1, verticalsize));
	erode(vertical, vertical, verticalStructure, Point(-1, -1));
	dilate(vertical, vertical, verticalStructure, Point(-1, -1));

	#ifdef SHOW_STEPS
		imshow("vertical", vertical);
	#endif

	bitwise_not(vertical, vertical);

	Mat edges;
	adaptiveThreshold(vertical, edges, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 3, -2);

	Mat kernel = Mat::ones(2, 2, CV_8UC1);
	dilate(edges, edges, kernel);

	Mat dest;
	vertical.copyTo(dest);

	blur(dest, dest, Size(2, 2));

	dest.copyTo(vertical, edges);

	#ifdef SHOW_STEPS
		imshow("smooth", dest);
	#endif

	vector<Mat> images;
	images.push_back(dest);
	images.push_back(horizontal);
	return images;
}
vector<Mat> _2GroupsSegmentation(Mat img, const int TH_ERROR, vector<int> lineRows,int nrLine) {
	Mat bw;
	adaptiveThreshold(~img, bw, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, -2);
	vector<Mat> images = removeHorizontalLines(bw);
	Mat imgRemLine = images[0];
	Mat horizontal = images[1];
	
	lineRows = indentifyLinePositions(images[1], 255, 0);
	#ifdef SHOW_STEPS
		cout << "Line row local coordonates for line nr.: " << nrLine;
		for (int i = 0; i < lineRows.size(); i++) {
			cout << lineRows[i] << " ";
		}
		cout << endl;
	#endif	

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
				whiteSpaces.push_back(nr);
			}
			width = 0;
		}
		if (i == verticalProj.size() - 1) {
			if (width >= TH_ERROR && width <= (img.cols/4)) {
				Rect nr(start, 0, width, imgRemLine.rows);
				whiteSpaces.push_back(nr);
			}
		}
	}
	vector<Mat> groups;
	//extract lineBoundingBox
	Rect lineBox = lineBoundingBoxes[nrLine];
	for (int i = 0; i < whiteSpaces.size() - 1; i++) {
		int newX = whiteSpaces[i].x + whiteSpaces[i].width-3;
		int newY = whiteSpaces[i].y;
		int newHeight = whiteSpaces[i].height;
		int newWidth = whiteSpaces[i+1].x - newX;
		
		if (newWidth >=TH_ERROR) {
			Rect noteRect(newX, newY, newWidth, newHeight);
			#ifdef SHOW_STEPS
				rectangle(imgRemLine, noteRect, 220, 1);
			#endif
			Mat roi = imgRemLine(noteRect);
			Rect noteBox(newX, newY + lineBox.y, newWidth, newHeight);
			groupBoundingBoxes[nrLine].push_back(noteBox);
			groups.push_back(roi);
		}
	}
	return groups;
}
vector<Mat> _2LinesSegmentation(Mat img,const int TH_ERROR) {
	vector<int> horizontalProj = horizontalProjection(img, 0, 1);
	//Mat horProj = viewProjection(horizontalProj, 0, img.rows, img.cols);

	Mat rectanguled = img.clone();

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
					lineBoundingBoxes.push_back(nr);
					#ifdef SHOW_STEPS
						rectangle(rectanguled, nr, 50, 2);
					#endif
					Mat roi = img(nr);
					lines.push_back(roi);
				}
				height = 0;
			}
		}
	}
	#ifdef SHOW_STEPS
		imshow("Rectanguled", rectanguled);
	#endif
	return lines;
}

int main() {
	Mat img = imread("musical_sheet/sheet7.jpg", 0);
	imshow("Original image", img);

	uchar threshhold = findThresholdValue(img, THRESHHOLD_ERROR);
	cout << (int)threshhold << endl;
	Mat binary = threshold(img, 200);


	vector<Mat> lines = _2LinesSegmentation(binary,LINE_SEG_ERROR);
	char lineName[30];
	char groupName[30];
	vector<vector<Mat>> groups(lines.size());
	vector<vector<int>> imageLines(lines.size());

	
	for (int i = 0; i < lines.size(); i++) {
		
		groups[i] = _2GroupsSegmentation(lines[i], GROUP_SEG_ERROR, imageLines[i],i);
		sprintf(lineName, "%s%d%s", "ready/line", i, ".bmp");
		imwrite(lineName, lines[i]);

		for (int j = 0; j < groups[i].size(); j++) {
			sprintf(groupName, "%s%d%s%d%s", "ready/line", i, "_group",j,".bmp");
			imwrite(groupName, groups[i][j]);
		}
	}

	int groupWidth = findMedianGroupWidth(groups);
	vector<vector<Mat>> notes = analizeAllGroups(groups, groupWidth);


	Mat dst = createFinalBoundingBoxes(img);
	imshow("Final Image", dst);
	cout << "end" << endl;
	waitKey(0);
	return 0;
}