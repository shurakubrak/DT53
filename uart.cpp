#include "device.h"
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <iostream>
#include <unistd.h>

using namespace std;

bool uart_t::open_uart()
{
	struct termios options;
	
	set_CTS();

	m_fd = open(COM, O_RDWR | O_NOCTTY | O_NDELAY);
	if (m_fd == -1) {
		cout << "Unable to open UART" << endl;
		return false;
	}

	tcgetattr(m_fd, &options);
	cfsetispeed(&options, B57600);
	cfsetospeed(&options, B57600);

	options.c_cflag |= (CLOCAL | CREAD | CRTSCTS | PARENB | PARODD);
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;

	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 100;	// Ten seconds (100 deciseconds)
	options.c_cc[VEOL] = 0;

	tcflush(m_fd, TCIFLUSH);
	tcsetattr(m_fd, TCSANOW, &options);
	return true;
}
//-----------------------------------------------

void uart_t::write_soft(char* buf, int size)
{
	int written = 0;
	while (written < size) {
		ssize_t result;
		if ((result = write(m_fd, buf, size - written)) < 0) {
			if (errno == EAGAIN) {
				struct pollfd pfd = { .fd = m_fd, .events = POLLOUT };
				if (poll(&pfd, 1, -1) <= 0 && errno != EAGAIN) {
					break;
				}
				continue;
			}
			return;
		}
		written += result;
		buf += result;
	}
	cout << "Write soft OK" << endl;
}
//------------------------------------------------

void uart_t::write_data(char* buf, int size)
{
	write(m_fd, buf, size);
	tcdrain(m_fd);
}
//-------------------------------------------

void uart_t::read_data()
{
	if (m_fd != -1)
	{
		// Read up to 255 characters from the port if they are there
		unsigned char rx_buffer[256];

		int rx_length = read(m_fd, (void*)rx_buffer, 255);
		if (rx_length > 0) {
			for (int i = 0; i < rx_length; i++)
				buffer_analize(rx_buffer[i]);
			rx_buffer[rx_length] = '\0';
		}
	}
}
//---------------------------------------------------

void uart_t::buffer_analize(uint8_t byte)
{
	string sendbuf = "";

	if (m_str.size() > 30) {
		m_begin = false;
		m_str = "";
	}
	else if (byte == BeginCOM) {
		m_begin = true;
		m_str = "";
	}
	else if (byte == EndCOM)
		m_begin = false;
	else if (m_begin)
		m_str.append(1, byte);
}
//-----------------------------------------------------

unsigned uart_t::gpio_base()
{
	/* adapted from bcm_host.c */
	unsigned address = ~0;
	FILE* fp = fopen("/proc/device-tree/soc/ranges", "rb");
	if (fp) {
		unsigned char buf[4];
		fseek(fp, 4, SEEK_SET);
		if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
			address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
		fclose(fp);
	}
	return (address == ~0 ? 0x20000000 : address) + GPIO_OFFSET;
}
//----------------------------------------

bool uart_t::set_CTS()
{
	int gfpsel, gpiomask;

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) 
		return false;
	
	void* gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, gpio_base());
	close(fd);
	if (gpio_map == MAP_FAILED) 
		return false;
	
	volatile unsigned* gpio = (volatile unsigned*)gpio_map;
	gfpsel = GFPSEL1;
	gpiomask = GPIO1617mask;
	gpio[gfpsel] |= gpiomask;
	return true;
}
//-----------------------------------------
