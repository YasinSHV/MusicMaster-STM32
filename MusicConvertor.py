#Convert Music notes from arduino format to desired format
#Use https://tuneform.com/tools/time-tempo-bpm-to-milliseconds-ms to convert duration
def convert_input(input_string):
    notes = input_string.split(',')
    formatted_notes = []
    for i in range(0, len(notes), 2):
        note = notes[i]
        duration = int(notes[i+1])
        formatted_notes.append(f'{{{note}, {duration}}}')
    return ', '.join(formatted_notes)

#Remove all white-spaces(you can use: https://www.browserling.com/tools/remove-all-whitespace)
#Remove all comments(you can use: https://www.removecomments.com/)
#example input(NOTE_E5,4,NOTE_B4,8,NOTE_C5,8)
input_string = "NOTE_E5,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,4,NOTE_C5,8,NOTE_B4,8,NOTE_A4,4,NOTE_A4,8,NOTE_C5,8,NOTE_E5,4,NOTE_D5,8,NOTE_C5,8,NOTE_B4,-4,NOTE_C5,8,NOTE_D5,4,NOTE_E5,4,NOTE_C5,4,NOTE_A4,4,NOTE_A4,8,NOTE_A4,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,-4,NOTE_F5,8,NOTE_A5,4,NOTE_G5,8,NOTE_F5,8,NOTE_E5,-4,NOTE_C5,8,NOTE_E5,4,NOTE_D5,8,NOTE_C5,8,NOTE_B4,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,4,NOTE_E5,4,NOTE_C5,4,NOTE_A4,4,NOTE_A4,4,REST,4,NOTE_E5,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,4,NOTE_C5,8,NOTE_B4,8,NOTE_A4,4,NOTE_A4,8,NOTE_C5,8,NOTE_E5,4,NOTE_D5,8,NOTE_C5,8,NOTE_B4,-4,NOTE_C5,8,NOTE_D5,4,NOTE_E5,4,NOTE_C5,4,NOTE_A4,4,NOTE_A4,8,NOTE_A4,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,-4,NOTE_F5,8,NOTE_A5,4,NOTE_G5,8,NOTE_F5,8,NOTE_E5,-4,NOTE_C5,8,NOTE_E5,4,NOTE_D5,8,NOTE_C5,8,NOTE_B4,4,NOTE_B4,8,NOTE_C5,8,NOTE_D5,4,NOTE_E5,4,NOTE_C5,4,NOTE_A4,4,NOTE_A4,4,REST,4,NOTE_E5,2,NOTE_C5,2,NOTE_D5,2,NOTE_B4,2,NOTE_C5,2,NOTE_A4,2,NOTE_GS4,2,NOTE_B4,4,REST,8,NOTE_E5,2,NOTE_C5,2,NOTE_D5,2,NOTE_B4,2,NOTE_C5,4,NOTE_E5,4,NOTE_A5,2,NOTE_GS5,2"
output_string = convert_input(input_string)
print(output_string)