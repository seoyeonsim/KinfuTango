/* 
 * pose_formatter.c
 *
 * INPUT: Single txt file containing n lines of Tango data in the format:
 *
 * 			timestamp quaternion translation
 *  		    t     x  y  z  w   x  y  z 
 *
 *		AND the color-code of the Tango device used (blue or black)
 *
 * OUTPUT: n txt files each containing one set of Tango data in the format:
 *
 * 			"TVector"
 * 			TX
 *			TY
 * 			TZ
 * 			
 * 			"RMatrix"
 * 			a	b	c
 * 			d 	e 	f
 * 			g 	h 	i
 *
 *			"Camera Intrinsics: focal height width"
 *			focal height width
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <glob.h>

#define WIDTH 8
#define MAX_CAPS 2000
#define NUM_CAPS 1056
#define NUM_LINES 474



int cmp(const void *x, const void *y) {

  double xx = *(double*)x, yy = *(double*)y;
  if (xx < yy) return -1;
  if (xx > yy) return  1;

 }


// getCapTimeArray retrieves a double array of the timestamps of all the
// jpg images in the given directory
double *getCapTimeArray() {

	DIR* FD;
	struct dirent* in_file;
	FILE *entry_file;
	int len;
	static double capTimes[NUM_CAPS];
	double time;
	int i = 0;

	if ((FD = opendir(".")) == NULL) {
		fprintf(stderr, "Error: couldn't open input directory\n");
		exit(1);
	}

	// if pattern is matched, put image timestamp into array
	while ((in_file = readdir(FD)) != NULL) {
		if (sscanf(in_file->d_name, "image_%07lf.jpg", &time) == 1) {
			capTimes[i] = time;
			i++;
		}
	}

	closedir(FD);

	// order the capTimes array to be sorted numerically
	qsort(capTimes, NUM_CAPS, sizeof(double), cmp);

	return capTimes;
}


// quaternionToMatrix takes in a double array (quaternion as x, y, z, w) and 
// outputs a double array (rotational matrix as a, b, c, d, e, f, g, h, i)
double *quaternionToMatrix (double quatern[]) {

	static double rMatrix[9];
	double x = quatern[0];
	double y = quatern[1];
	double z = quatern[2];
	double w = quatern[3];

	rMatrix[0] = 1 - (2*y*y) - (2*z*z);
	rMatrix[1] = (2*x*y) - (2*z*w);
	rMatrix[2] = (2*x*z) + (2*y*w);
	rMatrix[3] = (2*x*y) + (2*z*w);
	rMatrix[4] = 1 - (2*x*x) - (2*z*z);
	rMatrix[5] = (2*y*z) - (2*x*w);
	rMatrix[6] = (2*x*z) - (2*y*w);
	rMatrix[7] = (2*y*z) + (2*x*w);
	rMatrix[8] = 1 - (2*x*x) - (2*y*y);

	return rMatrix;
}


int main (int argc, char* argv[]) {

	FILE *poseFile, *tmpFile;
	double poseData[NUM_LINES][WIDTH];
	double *poseLine, *rMatrix, *quatern, *capTimes;
	double currTime, nextDiff, currDiff, closestCapTime;
	char tmpFilename[10], oldFileName[20], newFileName[20];
	int i, j, k, numCaps;
	double bluIntrins[3] = {1042.8, 720, 1280};
	double blaIntrins[3] = {1042.4, 720, 1280};
	int indCurr = -1;

	// 2 arguments: pose data txt file, tango device name
	if (argc != 3) {
		fprintf(stderr, "Invalid arguments\n");
		exit(1);
	}

	poseFile = fopen(argv[1], "r");
	if (!poseFile) return 1;


	// put all the data in a 2D array poseData
	for (i = 0; i < NUM_LINES; i++) {
		for (j = 0; j < WIDTH; j++) {
			fscanf(poseFile, "%lf", &poseData[i][j]);
		}
	}

	fclose(poseFile);

	// use getCapTimeArray fcn to retrieve double array of img cap timestamps
	capTimes = getCapTimeArray();
	numCaps = NUM_CAPS;


	// iterate through every line of pose data
	// convert quaternion to matrix
	// create new file for each line
	for (k = 0; k < 474; k++) {

		// setting everything up
		poseLine = poseData[k];
		currTime = poseLine[0];
		quatern = &poseLine[1];
		sprintf(tmpFilename, "%.3d.txt", k);
		fprintf(stderr, "%s\n", tmpFilename);

		// converting rotational data, creating and writing to file 'k'.txt
		rMatrix = quaternionToMatrix(quatern);
		tmpFile = fopen(tmpFilename, "w+");

		// writing actual file data
		fprintf(tmpFile, "TVector\n");
		fprintf(tmpFile, "%.13f\n%.13f\n%.13f\n\n", poseLine[5], poseLine[6], poseLine[7]);
		fprintf(tmpFile, "RMatrix\n");
		fprintf(tmpFile, "  %.13f  %.13f  %.13f\n", rMatrix[0], rMatrix[1], rMatrix[2]);
		fprintf(tmpFile, "  %.13f  %.13f  %.13f\n", rMatrix[3], rMatrix[4], rMatrix[5]);
		fprintf(tmpFile, "  %.13f  %.13f  %.13f\n\n", rMatrix[6], rMatrix[7], rMatrix[8]);
		fprintf(tmpFile, "Camera Intrinsics: focal height width\n");
		if (strcmp(argv[2], "blue") == 0) {
			fprintf(tmpFile, "%lf %lf %lf", bluIntrins[0], bluIntrins[1], bluIntrins[2]);
		}
		else if (strcmp(argv[2], "black") == 0) {
			fprintf(tmpFile, "%lf %lf %lf", blaIntrins[0], blaIntrins[1], blaIntrins[2]);
		}
		else {
			fprintf(stderr, "Invalid tango name\n");
		}


		indCurr += 1;

		// find image timestamp closest to currTime
		for (i = indCurr; i < numCaps - 1; i++) {

			currDiff = fabs(capTimes[i] - poseLine[0]);
			nextDiff = fabs(capTimes[i+1] - poseLine[0]);

			if (currDiff < nextDiff) {
				indCurr = i;
				closestCapTime = capTimes[i];
				break;
			}
		}	


		// rename image file titled image_<closestCapTime>.jpg to <k>.jpg
		sprintf(oldFileName, "image_%.2f.jpg", closestCapTime);
		fprintf(stderr, "%s\n", oldFileName);
		sprintf(newFileName, "%.03d.jpg", k);
		fprintf(stderr, "%s\n", newFileName);
		if (rename(oldFileName, newFileName) == -1) {
			fprintf(stderr, "Renaming of file failed\n");
			exit(1);
		}
	}


	return 0;
}
