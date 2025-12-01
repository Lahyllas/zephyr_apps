#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/header.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define STRING_BUFFER_LEN 50

#define PING_NODE_ID 1

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc); return 1;}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

std_msgs__msg__Header outcoming_ping;
std_msgs__msg__Header incoming_pong;

rcl_publisher_t ping_publisher;
rcl_subscription_t pong_subscriber;

uint32_t seq_no = 0;

void ping_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	(void)last_call_time;

	if (timer == NULL) {
		return;
	}

	seq_no++;
	sprintf(outcoming_ping.frame_id.data, "%u_%u", PING_NODE_ID, seq_no);
	outcoming_ping.frame_id.size = strlen(outcoming_ping.frame_id.data);
	
	// Fill the message timestamp
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	outcoming_ping.stamp.sec = ts.tv_sec;
	outcoming_ping.stamp.nanosec = ts.tv_nsec;

	rcl_publish(&ping_publisher, (const void*)&outcoming_ping, NULL);
}

void pong_subscription_callback(const void * msgin)
{
	const std_msgs__msg__Header * msg = (const std_msgs__msg__Header *)msgin;

	if (strcmp(outcoming_ping.frame_id.data, msg->frame_id.data) == 0) {
		struct timespec ts;
		uint64_t send_ts = (uint64_t)msg->stamp.sec * 1000000000 + msg->stamp.nanosec;
		clock_gettime(CLOCK_REALTIME, &ts);
		uint64_t recv_ts = ts.tv_sec * 1000000000 + ts.tv_nsec;
		uint64_t rtt = recv_ts - send_ts;
		printf("Pong(%s) RTT: %llu ns\n", msg->frame_id.data, rtt);
	}
}


void main(void)
{
	// Init micro-ROS
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node;
	RCCHECK(rclc_node_init_default(&node, "ping_node", "", &support));

	// Create a reliable ping publisher
	RCCHECK(rclc_publisher_init_default(&ping_publisher, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header), "/microROS/ping"));

	// Create a best effort pong subscriber
	RCCHECK(rclc_subscription_init_best_effort(&pong_subscriber, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Header), "/microROS/pong"));


	// Create a 2 seconds ping timer
	rcl_timer_t timer;
	RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(2000), ping_timer_callback));


	// Create executor
	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));
	RCCHECK(rclc_executor_add_subscription(&executor, &pong_subscriber, &incoming_pong, &pong_subscription_callback, ON_NEW_DATA));

	// Create and allocate the pingpong messages

	char outcoming_ping_buffer[STRING_BUFFER_LEN];
	outcoming_ping.frame_id.data = outcoming_ping_buffer;
	outcoming_ping.frame_id.capacity = STRING_BUFFER_LEN;

	char incoming_pong_buffer[STRING_BUFFER_LEN];
	incoming_pong.frame_id.data = incoming_pong_buffer;
	incoming_pong.frame_id.capacity = STRING_BUFFER_LEN;

	while(1){
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
		usleep(100000);
	}	
	
	RCCHECK(rcl_publisher_fini(&ping_publisher, &node));
	RCCHECK(rcl_subscription_fini(&pong_subscriber, &node));
	RCCHECK(rcl_node_fini(&node));
}
