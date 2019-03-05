EdgeOS: an operating system for the edge cloud
==================================

In the future, computing will be immersed in the world around us – from augmented reality to autonomous vehicles to the
Internet of Things. Many of these smart devices will offer services that respond in real time to their physical
surroundings, requiring complex processing with strict performance guarantees. Edge clouds promise a pervasive
computational infrastructure a short network hop away from end devices, but today’s operating systems are a poor fit to
meet the goals of scalable isolation, dense multi-tenancy, and predictable performance required by these emerging
applications. In this paper we present EdgeOS, a micro-kernel based operating system that meets these goals by blending
recent advances in real-time systems and network function virtualization. EdgeOS introduces a Featherweight Process
model that offers lightweight isolation and supports extreme scalability even under high churn. Our architecture
provides efficient communication mechanisms, and low-overhead per-client isolation. To achieve high performance
networking, EdgeOS employs kernel bypass paired with the isolation properties of Featherweight Processes. We have
evaluated our EdgeOS prototype for running high scale network middleboxes using the Click software router and endpoint
applications using memcached. EdgeOS reduces startup latency by 170X compared to Linux processes and over five orders of
magnitude compared to containers, while providing three orders of magnitude latency improvement when running 300 to 1000
edge-cloud memcached instances on one server.
