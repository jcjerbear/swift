//switch statements
enum Colour{
  case yellow, red, blue, green, black
}
let c = Colour.red 
var colourName: String
switch(c){
  case .yellow: colourName = "yellow"
  case .red:    colourName = "red"
  case .blue:   colourName = "blue"
  case .gree:   colourName = "green"
  case .black:  colourName = "black"
  default: colourName = "Invalid Colour"
}
print("The colour is: \(colourName)")

