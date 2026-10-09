#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main()
{
    // 1. Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // 2. Create links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Connect Node 0 to Node 1
    NetDeviceContainer devices01;
    devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

    // 4. Connect Node 1 to Node 2
    NetDeviceContainer devices12;
    devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // 5. Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 6. Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01;
    interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12;
    interfaces12 = address.Assign(devices12);

    // 7. Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. Create UDP server on Node 2
    uint16_t port = 5000;

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // 9. Create UDP client on Node 0
    UdpClientHelper client(interfaces12.GetAddress(1), port);

    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // 10. Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(
        flowMonitorHelper.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats =
    	flowMonitor->GetFlowStats();

    
    for (auto const& flow : stats)
{
    const auto& s = flow.second;

    // 1. Calculate packet loss
    uint64_t lostPackets = s.txPackets - s.rxPackets;
    double packetLossPercent = 0.0;

    if (s.txPackets > 0)
    {
        packetLossPercent =
            (static_cast<double>(lostPackets) / s.txPackets) * 100.0;
    }

    // 2. Calculate average end-to-end delay
    double averageDelayMs = 0.0;

    if (s.rxPackets > 0)
    {
        averageDelayMs =
            (s.delaySum.GetSeconds() / s.rxPackets) * 1000.0;
    }

    // 3. Calculate received throughput
    double throughputMbps = 0.0;

    if (s.rxPackets > 0)
    {
        double duration =
            (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();

        if (duration > 0.0)
        {
            throughputMbps =
                (static_cast<double>(s.rxBytes) * 8.0)
                / duration / 1e6;
        }
    }

    // 4. Print results
    std::cout << "\n========== Flow " << flow.first
              << " ==========\n";

    std::cout << "Packets Sent: "
              << s.txPackets << '\n';

    std::cout << "Packets Received: "
              << s.rxPackets << '\n';

    std::cout << "Packets Lost: "
              << lostPackets << '\n';

    std::cout << "Packet Loss: "
              << packetLossPercent << "%\n";

    std::cout << "Average Delay: "
              << averageDelayMs << " ms\n";

    std::cout << "Throughput: "
              << throughputMbps << " Mbps\n";
}


    Simulator::Destroy();

    return 0;
}
