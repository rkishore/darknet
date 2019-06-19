validation_filenames = ["Axis_P1425-LE_Vis_Day_Scenario1-2.ts_frames",\
                        "Axis_P1425-LE_Vis_Evening_ScenarioE1-1.ts_frames",\
                        "SSIII_IR_Day_Scenario1-2.ts_frames",\
                        "SSIII_IR_Evening_ScenarioE1-2.ts_frames",\
                        "SSIII_IR_Night_ScenarioN1-3.ts_frames",\
                        "Day_Vig4_Open3_HDLL_HD380.ts_frames",\
                        "Day_Vig5_Open1_SWIR_HD380C.ts_frames",\
                        "Day_Vig6_Open2_HDEO_HD380C.ts_frames",\
                        "Day_Vig5_Open3_HDEO_HD380.ts_frames",\
                        "Scout_Recon_IR_Day_Scenario1-2.ts_frames",\
                        "Scout_Recon_IR_Night_ScenarioN1-3.ts_frames",\
                        "Scout_Recon_IR_Night_ScenarioN3-2.ts_frames",\
                        "Scout_Sony_Vis_Evening_ScenarioE1-3.ts_frames",\
                        "Pelco_Vis_Day_Scenario1-2.ts_frames",\
                        "Pelco_Vis_Day_Scenario2-1.ts_frames",]
    
# Test set (we have detection results without training for these videos)
# json_filenames_to_notprocess = []
test_filenames = ["Axis_P1425-LE_Vis_Day_Scenario1-1.ts_frames",\
                  "Day_Vig4_Open2_HDLL_HD380.ts_frames",\
                  "Day_Vig4_Open1_HDEO_HD380C.ts_frames",\
                  "Night_Vig6_Open3_HDIR_HD380_WFOV.ts_frames",\
                  "Night_Vig4_Open2_HDIR_HD380.ts_frames",\
                  "Pelco_Vis_Day_Scenario1-1.ts_frames",\
                  "SSIII_IR_Evening_ScenarioE1-1.ts_frames",\
                  "Scout_Recon_IR_Day_Scenario1-1.ts_frames",\
                  "Scout_Recon_IR_Evening_ScenarioE1-1.ts_frames",\
                  "Scout_Recon_IR_Night_ScenarioN3-1.ts_frames",\
                  "Scout_Sony_Vis_Evening_ScenarioE1-1.ts_frames",\
                  "Scout_Sony_Vis_Day_Scenario3-1.ts_frames",]

unique_keys = {}
total_vehicles = 0
total_val = 0
total_test = 0
with open("/tmp/person.txt", "r") as ifp:
	for line in ifp:
		split_line = line.split("/")
		for split_line_com in split_line:
			if ".json" in split_line_com:
				split_filename = split_line_com.split(".json")
                                if split_filename[0] in validation_filenames:
                                        total_val += 1
                                if split_filename[0] in test_filenames:
                                        total_test += 1        
				if split_filename[0] not in unique_keys.keys():
					unique_keys[split_filename[0]] = 1
				else:
					unique_keys[split_filename[0]] += 1
				total_vehicles += 1
				break

print "Total files: %d | total persons: " % len(unique_keys.keys()), total_vehicles, "val: %d (%0.2f%%), test: %d (%0.2f%%)" % (total_val, (total_val*100.0)/total_vehicles, total_test, (total_test*100.0)/total_vehicles,)
for key in unique_keys.keys():
	print key, unique_keys[key]
