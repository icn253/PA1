/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Isabelle Nguyen
	UIN: 533008056
	Date: 9/26/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>  // for wait()


using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
    double t = -1;
    int e = -1;
	int mval = MAX_MESSAGE;  // default value for -m
    bool use_new_chan = false;

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c:")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				mval = atoi(optarg);
				break;
			case 'c':
				use_new_chan = true;
				break;		
		}
	}
	//give arguments for the server
	//server needs './server', '-m', '<val for -m arg>', 'NULL'
	//fork
	//In the child run execvp using the server arguments
	//fork() to make a child
	char m_str[16];
	sprintf(m_str, "%d", mval);  // converts 256 -> "256"

	char* args[] = {(char*) "./server", (char*) "-m", m_str, nullptr};

	pid_t pid = fork();
	if (pid == -1){
		cerr << "fork failed\n";
		return 1;
	}
	if (pid == 0){ //child process
		execvp(args[0], args);
		cerr << "execvp failed\n";
		return 0;
	}
	//new channel
    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

	FIFORequestChannel* active_chan = &chan;
    FIFORequestChannel* extra_chan = nullptr;
	
	if( use_new_chan){
		MESSAGE_TYPE new_channel_request = NEWCHANNEL_MSG;
		chan.cwrite(&new_channel_request, sizeof(MESSAGE_TYPE));

		char new_channel_name[100];
		chan.cread(new_channel_name, sizeof(new_channel_name));

		extra_chan = new FIFORequestChannel (new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		active_chan = extra_chan;
	}

	//FUNCTIONALITY
	if (!filename.empty()){
		// sending a non-sense message, you need to change this
		filemsg fm(0, 0);
		string fname = filename;

		int len = sizeof(filemsg) + (fname.size() + 1); //+1 for null terminator at end
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		active_chan->cwrite(buf2, len);  // I want the file length;

		int64_t filesize = 0;
		active_chan->cread(&filesize, sizeof(int64_t)); //recieves the file length

		char* buf3 =  new char[mval];//create buffer of size buff capacity(m)
		ofstream fout("received/" + fname, ios::binary); //open and output to file
		int64_t offset = 0;
		//loop over the segments in the file filesize/buff capacity(m)
		while (offset < filesize){
			int chunk_size = min<int64_t>(mval, filesize - offset); //set chunk size, careful of the end
			
			filemsg* file_req = (filemsg*)buf2; //create filemsg instance
			file_req->offset = offset; //set offset in the file, where are we starting from?
			file_req->length = chunk_size; //set the length. Be careful of the last segment, # bytes to read
			
			active_chan->cwrite(buf2, len); //send the request (buf2)
			active_chan->cread(buf3, file_req->length); //receive the response, cread into buf3 length file_req->len

			fout.write(buf3, chunk_size); //write buf3 into file: received/filename
			offset += chunk_size;
		}

		cout << "File length: " << filesize << " bytes" << endl;
		delete[] buf2;
		delete[] buf3;

	}else if (t != -1 && p != -1 && e != -1){ //if p, t, e are specified
		//single point
		// example data point request
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e); //change from hard coding to user's values
		
		memcpy(buf, &x, sizeof(datamsg));
		active_chan->cwrite(buf, sizeof(datamsg)); // question
		double reply;
		active_chan->cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}else if(p != -1){ // if p is specified, t and e not specified

		ofstream out("received/x1.csv");
		for (int i = 0; i < 1000; ++i) { //loop over first 1000 lines
			t = i * 0.004;
			for (int ecg = 1; ecg <= 2; ++ecg) {
				datamsg msg(p, t, ecg);
				char buf[MAX_MESSAGE];
				memcpy(buf, &msg, sizeof(datamsg));
				active_chan->cwrite(buf, sizeof(datamsg));//question

				double reply;
				active_chan->cread(&reply, sizeof(double)); //answer

				//cout << "Requesting point " << i << " ECG" << ecg << endl;
				if (ecg == 1) out << t << "," << reply << ",";
				else out << reply << "\n";
			}
		}
		out.close();
	}

	// closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;
	active_chan->cwrite(&m, sizeof(MESSAGE_TYPE));
	if (extra_chan) delete extra_chan;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));

	int status;
    wait(&status); //wait for child server to exit
	return 0;
}
