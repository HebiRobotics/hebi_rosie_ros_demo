#!/usr/bin/env python

import rospy

from std_msgs.msg import Float64
from hebi_rosie_demo.srv import GripperSrv, GripperSrvResponse


def update_gripper(req):
    global effort_msg
    if req.closed:
        effort_msg.data = 1.0
    else:
        effort_msg.data = -1.0
    return GripperSrvResponse()


if __name__ == "__main__":
    effort_msg = Float64()
    effort_msg.data = -1.0

    rospy.init_node("gripper_service_node")
    rospy.Service("close", GripperSrv, update_gripper)
    effort_pub = rospy.Publisher("gripper_strength", Float64, queue_size=5)

    r = rospy.Rate(50)
    while not rospy.is_shutdown():
        effort_pub.publish(effort_msg)
        r.sleep()
