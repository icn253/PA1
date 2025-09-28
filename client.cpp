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
	int p = 1;
    double t = 0.0;
    int e = 1;
	int mval = 256;  // default value for -m

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:")) != -1) {
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
		}
	}
	//give arguments for the server
	//server needs './server', '-m', '<val for -m arg>', 'NULL'
	//fork
	//In the child run execvp using the server arguments
	//fork() to make a child
	char m_str[16];
	sprintf(m_str, "%d", mval);  // converts 256 -> "256"

	char* server_args[] = {(char*) "./server", (char*) "-m", m_str, nullptr};

	pid_t pid = fork();
	if (pid == -1){
		cerr << "fork failed\n";
		return 1;
	}
	if (pid == 0){ //child process
		execvp(server_args[0], server_args);
		cerr << "execvp failed\n";
		return 0;
	} else{
		sleep(1);
	}
	//new channel
    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
	
	MESSAGE_TYPE new_channel_request = NEWCHANNEL_MSG;
	chan.cwrite(&new_channel_request, sizeof(MESSAGE_TYPE));

	char new_channel_name[100];
	chan.cread(new_channel_name, sizeof(new_channel_name));

	FIFORequestChannel new_chan(new_channel_name, FIFORequestChannel::CLIENT_SIDE);


	ofstream out("x1.csv");

	for (int i = 0; i < 1000; ++i) {
		t = i * 0.004;
		for (int ecg = 1; ecg <= 2; ++ecg) {
			datamsg msg(p, t, ecg);
			char buf[MAX_MESSAGE];
			memcpy(buf, &msg, sizeof(datamsg));
			new_chan.cwrite(buf, sizeof(datamsg));

			double reply;
			new_chan.cread(&reply, sizeof(double));

			//cout << "Requesting point " << i << " ECG" << ecg << endl;
			out << reply;
			if (ecg == 1) out << ",";
			else out << "\n";
		}
	}

	out.close();



	
	// example data point request
    char buf[MAX_MESSAGE]; // 256
    datamsg x(p, t, e); //change from hard coding to user's values
	
	memcpy(buf, &x, sizeof(datamsg));
	new_chan.cwrite(buf, sizeof(datamsg)); // question
	double reply;
	new_chan.cread(&reply, sizeof(double)); //answer
	cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	

    // sending a non-sense message, you need to change this
	filemsg fm(0, 0);
	string fname = filename;
	
	int len = sizeof(filemsg) + (fname.size() + 1);
	char* buf2 = new char[len];
	memcpy(buf2, &fm, sizeof(filemsg));
	strcpy(buf2 + sizeof(filemsg), fname.c_str());
	new_chan.cwrite(buf2, len);  // I want the file length;

	__int64_t file_length;
    new_chan.cread(&file_length, sizeof(__int64_t));
    cout << "File length: " << file_length << " bytes" << endl;

	delete[] buf2;
	
	 ofstream fout("received/" + filename, ios::binary);

    // Step 3: Request the file in chunks
    __int64_t offset = 0;
    int buffer_size = MAX_MESSAGE - sizeof(filemsg); // max data per message

    while (offset < file_length) {
        int chunk_size = min(buffer_size, (int)(file_length - offset));
        filemsg fm_chunk(offset, chunk_size);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* buf_chunk = new char[len];
        memcpy(buf_chunk, &fm_chunk, sizeof(filemsg));
        strcpy(buf_chunk + sizeof(filemsg), filename.c_str());

        new_chan.cwrite(buf_chunk, len);
        delete[] buf_chunk;

        // read the chunk from server
        char* data = new char[chunk_size];
        new_chan.cread(data, chunk_size);
        fout.write(data, chunk_size);
        delete[] data;

        offset += chunk_size;
    }

    fout.close();
    cout << "File received successfully: received/" << filename << endl;


	// closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;
	new_chan.cwrite(&m, sizeof(MESSAGE_TYPE));
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));

	int status;
    wait(&status); //wait for child server to exit
	return 0;
}
