from decoders import interleaved as decoder
import csv
import sys


fieldnames = ['macid', 'rssi', 'bandwith', 'seqnum', 'fragnum', 'coreNspatialstream' , 'framectrl', 'csi_len', 'csi']

def print_csi(samples, index):
    macid = samples.get_mac(index).hex()
    macid = ':'.join([macid[i:i+2] for i in range(0, len(macid), 2)])

    # Sequence control
    sc, fn = samples.get_seq(index)

    # Core and Spatial Stream
    css = samples.get_css(index).hex()

    rssi = samples.get_rssi(index)
    fctl = samples.get_fctl(index)

    csi_list = list(samples.get_csi(index,rm_nulls=False,rm_pilots=False))

    return {'macid': macid, 'rssi': rssi, 'bandwith': samples.bandwidth, 
    'seqnum': sc, 'fragnum': fn, 'coreNspatialstream': css, 'framectrl': fctl, 'csi_len': len(csi_list), 'csi': csi_list}






if __name__ == '__main__':

 
    # total arguments
    n = len(sys.argv)

    if n != 2:
        print("Usage: python storecsi.py <pcapfile, relative to pcapfiles/>")
        exit()

    # pcap file
    file_name = ""
    
    if ".pcap" in sys.argv[1]:
        file_name = sys.argv[1].split(".")[0]
    

    samples = decoder.read_pcap('pcapfiles/' + file_name + ".pcap")

    
    with open('csv/'+ file_name + ".csv", 'w', newline='') as f:

        csv_writer = csv.DictWriter(f, fieldnames=fieldnames)

        csv_writer.writeheader()

        for i in range(0, samples.nsamples):
            csv_writer.writerow(print_csi(samples, i))

