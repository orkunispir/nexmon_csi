import pandas as pd

if __name__ == '__main__':
    csv_file = pd.read_csv('csv/output_channel_36.csv')


    csv_file = csv_file[csv_file.macid == '0a:c2:c5:5f:b3:61']

    csv_file.to_csv('csv/output_channel_36_filtered.csv', index=False)