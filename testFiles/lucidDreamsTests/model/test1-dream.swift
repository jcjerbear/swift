//creating structure
//initialize / constructor

struct Person{
  var name: String
  init(_ name:String){
    self.name = name
  }
}

var p1 = Person("Yaser")

print(p1.name)

