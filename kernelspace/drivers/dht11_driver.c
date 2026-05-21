#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

struct dht11_data {
    struct gpio_desc *gpio;
    u8 buf[5];
};

/* ── Read 1 byte (8 bits) từ DHT11 ── */
static int dht11_read_byte(struct gpio_desc *gpio)
{
    int i, byte = 0;
    int timeout;

    for (i = 0; i < 8; i++) {
        /* Wait for HIGH — bắt đầu bit */
        timeout = 200;
        while (!gpiod_get_value(gpio) && timeout--)
            udelay(1);
        if (timeout <= 0) return -1;

        /* Đợi 30µs để sample bit */
        udelay(30);

        if (gpiod_get_value(gpio))
            byte = (byte << 1) | 1;
        else
            byte = (byte << 1);

        /* Wait for LOW — kết thúc bit */
        timeout = 200;
        while (gpiod_get_value(gpio) && timeout--)
            udelay(1);
        if (timeout <= 0) return -1;
    }
    return byte;
}

/* ── Reset GPIO về trạng thái an toàn ── */
static void dht11_reset_gpio(struct gpio_desc *gpio)
{
    gpiod_direction_input(gpio);
    msleep(20);  /* chờ line stable về HIGH nhờ pull-up */
}

static int dht11_read_sensor(struct dht11_data *data)
{
    unsigned long flags;
    int i, val, timeout;

    /* QUAN TRỌNG: Khóa ngắt toàn cục ngay từ đầu */
    local_irq_save(flags); 

    /* Gửi Start Signal */
    gpiod_direction_output(data->gpio, 0);
    mdelay(20); // Dùng mdelay để CPU không chuyển sang tiến trình khác
    
    gpiod_direction_output(data->gpio, 1);
    udelay(40);
    gpiod_direction_input(data->gpio);
    
    /* Chờ phản hồi từ Sensor */
    timeout = 500;
    while (gpiod_get_value(data->gpio) && timeout--) udelay(1);
    if (timeout <= 0) goto fail;

    /* Chờ Sensor kéo HIGH */
    timeout = 500;
    while (!gpiod_get_value(data->gpio) && timeout--) udelay(1);
    if (timeout <= 0) goto fail;

    /* Chờ Sensor kéo LOW để bắt đầu data */
    timeout = 500;
    while (gpiod_get_value(data->gpio) && timeout--) udelay(1);

    /* Đọc 5 bytes dữ liệu */
    for (i = 0; i < 5; i++) {
        val = dht11_read_byte(data->gpio);
        if (val < 0) goto fail;
        data->buf[i] = (u8)val;
    }

    local_irq_restore(flags); // Mở lại ngắt sau khi xong
    dht11_reset_gpio(data->gpio);
    
    /* Kiểm tra Checksum */
    if (data->buf[4] != ((data->buf[0] + data->buf[1] + data->buf[2] + data->buf[3]) & 0xFF)) {
        return -2;
    }
    return 0;

fail:
    local_irq_restore(flags);
    dht11_reset_gpio(data->gpio);
    return -1;
}

/* ── File Operations ── */
static int dht11_open(struct inode *inode, struct file *file)
{
    struct miscdevice *mdev = file->private_data;
    struct dht11_data *data = dev_get_drvdata(mdev->parent);
    file->private_data = data;
    return 0;
}

/* File operations: Xóa các hàm không cần thiết để tối giản */
static ssize_t dht11_read_file(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    struct dht11_data *data = file->private_data;
    char out_str[128];
    int len, ret;

    if (*ppos > 0) return 0;

    ret = dht11_read_sensor(data);
    if (ret == 0) {
        len = sprintf(out_str, "Humidity: %d.%d%% Temperature: %d.%dC\n",
                      data->buf[0], data->buf[1], data->buf[2], data->buf[3]);
    } else if (ret == -2) {
        len = sprintf(out_str, "Error: Checksum failed\n");
    } else {
        len = sprintf(out_str, "Error: Sensor timeout\n");
    }

    if (copy_to_user(user_buf, out_str, len)) return -EFAULT;
    *ppos += len;
    return len;
}

static const struct file_operations dht11_fops = {
    .owner = THIS_MODULE,
    .read  = dht11_read_file,
    .open  = dht11_open,
};

static struct miscdevice dht11_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "my_dht11",
    .fops  = &dht11_fops,
};

/* ── Platform Driver ── */
static int dht11_probe(struct platform_device *pdev)
{
    struct dht11_data *data;
    int ret;

    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    data->gpio = devm_gpiod_get(&pdev->dev, NULL, GPIOD_IN);
    if (IS_ERR(data->gpio)) {
        pr_err("DHT11: failed to get GPIO\n");
        return PTR_ERR(data->gpio);
    }

    platform_set_drvdata(pdev, data);

    dht11_misc.parent = &pdev->dev;

    ret = misc_register(&dht11_misc);
    if (ret) {
        pr_err("DHT11: misc_register failed\n");
        return ret;
    }

    pr_info("DHT11: Driver probed, /dev/my_dht11 created\n");
    return 0;
}

static int dht11_remove(struct platform_device *pdev)
{
    misc_deregister(&dht11_misc);
    pr_info("DHT11: Driver removed\n");
    return 0;
}

static const struct of_device_id dht11_of_match[] = {
    { .compatible = "custom,my-dht11" },
    { }
};
MODULE_DEVICE_TABLE(of, dht11_of_match);

static struct platform_driver dht11_driver = {
    .driver = {
        .name           = "my_custom_dht11",
        .of_match_table = dht11_of_match,
    },
    .probe  = dht11_probe,
    .remove = dht11_remove,
};

module_platform_driver(dht11_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("thangdd");