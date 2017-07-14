/*
 *  A pseude devide to translate a string into morse code
 *
 *  This program code was written for the IT Arch. class
 *  in Future University Hakodate.
 *
 *  Copyright (C) 2017 Katsuya Matsubara.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define MORSEDEV_BUFLEN	30
/* ring buffer to store morse strings */
static unsigned char morsedev_buff[MORSEDEV_BUFLEN];
/* read pointer for the ring buffer */
static unsigned int buff_rpos;
/* write pointer for the ring buffer */
static unsigned int buff_wpos;
/* bytes of data stored in the buffer */
static size_t buff_n_data;

static int morsedev_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "morse: device open.\n");
	return 0;
}

static int morsedev_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "morse: device released.\n");
	return 0;
}

static ssize_t morsedev_read(struct file *filp, char __user * buff,
			     size_t count, loff_t * offp)
{
	size_t n_filled = 0;

	printk(KERN_INFO "morse: read from device.\n");

	if ((buff_n_data > 0) && (count > 0)) {
		size_t len = (buff_wpos > buff_rpos) ?
		    buff_wpos - buff_rpos : MORSEDEV_BUFLEN - buff_rpos;
		unsigned long remain;

		if (count < len)
			len = count;

		remain = copy_to_user(buff, &morsedev_buff[buff_rpos], len);
		if (remain > 0)
			printk(KERN_WARNING "morse: %ld bytes remain "
			       "in the buffer.\n", remain);

		len -= remain;
		buff_n_data -= len;
		count -= len;
		buff_rpos = (buff_rpos + len) % MORSEDEV_BUFLEN;
		buff += len;
		n_filled += len;
	}

	return n_filled;
}

static inline const unsigned char *char2morse(unsigned char c, size_t * slen)
{
	const unsigned char *morse_nums[10] = {
		/* 0 */ "----- ",
		/* 1 */ ".---- ",
		/* 2 */ "..--- ",
		/* 3 */ "...-- ",
		/* 4 */ "....- ",
		/* 5 */ "..... ",
		/* 6 */ "-.... ",
		/* 7 */ "--... ",
		/* 8 */ "---.. ",
		/* 9 */ "----. ",
	};

	const unsigned char *morse_alphabet[26] = {
			/* A */ ".- ",
			/* B */ "-... ",
			/* C */ "-.-. ",
			/* D */ "-.. ",
			/* E */ ". ",
			/* F */ "..-. ",
			/* G */ "--. ",
			/* H */ ".... ",
			/* I */ ".. ",
			/* J */ ".--- ",
			/* K */ "-.- ",
			/* L */ ".-.. ",
			/* M */ "-- ",
			/* N */ "-. ",
			/* O */ "--- ",
			/* P */ ".--. ",
			/* Q */ "--.- ",
			/* R */ ".-. ",
			/* S */ "... ",
			/* T */ "- ",
			/* U */ "..- ",
			/* V */ "...- ",
			/* W */ ".-- ",
			/* X */ "-..- ",
			/* Y */ "-.-- ",
			/* Z */ "--.. ",
	};

	const unsigned char *morse_double_quote = ".-..-. ";/* " */

	const unsigned char *morse_symbol[9] = {
			/* ' */ ".----. ",
			/* ( */ "-.--. ",
			/* ) */ "-.--.- ",
			/* * */ "-..- ",
			/* + */ ".-.-. ",
			/* , */ "--..-- ",
			/* - */ "-....- ",
			/* . */ ".-.-.- ",
			/* / */ "-..-. ",
	};

	const unsigned char *morse_colon = "---... ";/* : */
	const unsigned char *morse_equal = "-...- ";/* = */
	const unsigned char *morse_question = "..--.. ";/* ? */
	const unsigned char *morse_atmark = ".--.-, ";/* @ */

	const unsigned char *s;

	printk(KERN_INFO "morse: write from device.\n");

	if ((c >= '0') && (c <= '9')) {// c is '0'-'9'
			s = morse_nums[c - '0'];
	} else if ((c >= 'a') && (c <= 'z')) {
			s = morse_alphabet[c - 'a'];
	} else if ((c >= 'A') && (c <= 'Z')) {
			s = morse_alphabet[c - 'A'];
	} else if (c == '\"') {// c is \"
			s = morse_double_quote;
	} else if ((c >= '\'') && (c <= '/')) {// c is \', (, ), *, +, \,, -, ., /
			s = morse_symbol[c - '\''];
	} else if (c == ':') {
			s = morse_colon;
	} else if (c == '=') {
			s = morse_equal;
	} else if (c == '\?') {
			s = morse_question;
	} else if (c == '@') {
			s = morse_atmark;
	} else {
		printk(KERN_INFO "morse: skip a unsupported "
		       "character '%c'\n", c);
		s = NULL;
	}

	*slen = (s != NULL) ? strlen(s) : 0;
	return s;
}

static ssize_t morsedev_write(struct file *filp, const char __user * buff,
			      size_t count, loff_t * offp)
{
	int n;

	for (n = 0; n < count; n++) {
		unsigned char c;
		const unsigned char *morse_str;
		size_t morse_strlen;

		if (get_user(c, buff++) != 0) {
			printk(KERN_ERR "morse: get_user failed.\n");
			return -EINVAL;
		}

		morse_str = char2morse(c, &morse_strlen);
		if (morse_str == NULL)
			continue;

		if ((MORSEDEV_BUFLEN - buff_n_data) < morse_strlen)
			break;

		do {
			morsedev_buff[buff_wpos] = *morse_str++;
			buff_wpos = (buff_wpos + 1) % MORSEDEV_BUFLEN;
			buff_n_data++;
		} while (--morse_strlen > 0);
	}

	return n == 0 ? -ENOSPC : n;
}

static struct file_operations morsedev_fops = {
	.owner = THIS_MODULE,
	.open = morsedev_open,
	.read = morsedev_read,
	.write = morsedev_write,
	.release = morsedev_release,
};

static struct cdev *cdev;
static const dev_t dev = MKDEV(240, 0);

static int __init morse_init(void)
{
	int err;

	err = register_chrdev_region(dev, 1, "morse");
	if (err) {
		printk(KERN_ERR "morse: register_chrdev_region failed.\n");
		return -1;
	}

	cdev = cdev_alloc();
	if (!cdev) {
		printk(KERN_ERR "morse: cdev_alloc failed.\n");
		return -1;
	}

	cdev->ops = &morsedev_fops;
	err = cdev_add(cdev, dev, 1);
	if (err) {
		printk(KERN_ERR "morse: cdev_add failed.\n");
		return -1;
	}

	printk(KERN_INFO "morse: device is ready for I/O.\n");

	return err;
}

static void __exit morse_exit(void)
{
	if (cdev)
		cdev_del(cdev);
	unregister_chrdev_region(dev, 1);

	printk(KERN_INFO "morse: device removed.\n");
	return;
}

module_init(morse_init);
module_exit(morse_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Katsuya Matsubara");
