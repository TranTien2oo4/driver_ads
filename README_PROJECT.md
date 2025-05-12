BÀI TẬP NHÚNG : HCMUTE	 
-
GV.BÙI HÀ ĐỨC		
-
NHÓM:                                   
-
TRẦN VĂN TIẾN       22146417 ;                                    
ĐẶNG HỮU KIỆN       22146337 ;                                    
NGUYỄN DUY TRƯỜNG   22146436 ;                                    
CA TẤN DƯƠNG        22146289 ;                                   

------------------------------
Driver ADS1113 này dùng để đọc chip ADS1115 và trả về giá trị điện áp tại chân được chọn cho chương trình gọi. Ứng dụng ví dụ sẽ đọc chip ADS1115 và in ra màn hình các giá trị điện áp từ các chân a0 đến a3.

Các thiết bị I2C trong nhân Linux được hỗ trợ thông qua hệ thống con I2C, cho phép giao tiếp với các thiết bị phụ (slave) 
bằng nhiều hàm hỗ trợ và lớp trừu tượng. Thông qua các API trong kernel, Ta có thể kiểm soát hoàn toàn 
việc truyền dữ liệu I2C, cấp phát bộ nhớ và đăng ký driver.

#include <linux/init.h>                                   
#include <linux/module.h>                                   
#include <linux/ioctl.h>                                   
#include <linux/fs.h>                                   
#include <linux/device.h>                                   
#include <linux/err.h>                                   
#include <linux/list.h>                                   
#include <linux/errno.h>                                   
#include <linux/mutex.h>                                   
#include <linux/slab.h>                                   
#include <linux/uaccess.h>                                   
#include <linux/delay.h>                                   
#include <linux/i2c.h>                                   
                                   
Một số lý do khiến bạn có thể muốn sử dụng giao diện lập trình trong kernel bao gồm:

	Viết các driver cấp thấp tương tác trực tiếp với thiết bị I2C 
	thông qua các cấu trúc như i2c_client, i2c_driver và các hàm liên quan.

	Triển khai các giao thức hoặc driver thiết bị cần tích hợp với mô hình thiết bị của Linux, 
	quản lý bộ nhớ, cơ chế đồng bộ hoặc truy cập vào các chức năng chỉ có trong nhân.

Tất nhiên, có những driver không thể viết ở không gian người dùng (userspace), vì chúng cần truy cập vào các tài nguyên 
như đăng ký bus I2C, xử lý ngắt, hoặc các hàm liên quan đến thời gian mà chỉ có thể sử dụng trong nhân hệ điều hành



