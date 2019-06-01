# Import all libraries needed for the tutorial
import os

import nltk
from nltk import pos_tag

# General syntax to import specific functions in a library: 
##from (library) import (specific library function)
from pandas import DataFrame, read_csv

# General syntax to import a library but no functions: 
##import (library) as (give the library a nickname/alias)
import matplotlib.pyplot as plt
import pandas as pd #this is how I usually import pandas
import sys #only needed to determine Python version number
import matplotlib #only needed to determine Matplotlib version number

import wordcloud
from nltk.stem import PorterStemmer

# Enable inline plotting
#%matplotlib inline

# removed @ and #
my_punctuation = '!"$%&\'()*+,-./:;<=>?[\\]^_`{|}~'

def remove_unnecessary_words(str):
    #FIND A WAY TO REMOVE EMOJIS
    if len(str) > 0 and str[0] != '@' and str[0] != '#':
        if len(str) > 4 and str[0:4] != 'http':
            return True
        elif len(str) <= 4:
            return True
        else:
            return False
    else:
        return False

def unify_negations(words):
    new_words = []

    for word in words:
        if word == "n't":
            new_words_len = len(new_words)
            new_words[new_words_len - 1] = new_words[new_words_len - 1] + "n't"
        else:
            new_words.append(word)

    return new_words

path = '../twitter_data/train2017.tsv' # 'Experimental_Data.tsv'

exp_datafile = open(path,"r")

line = exp_datafile.readline()
lines = []

while len(line) != 0:
    line = line.split("\t")

    line[len(line) - 1] = line[len(line) - 1].replace('\n','')

    if len(line) > 0:
        lines.append(line)

    line = exp_datafile.readline()

my_dataframe = pd.DataFrame(data = lines,columns = ['ID1','ID2','Tag','Tweet'])

# Getting only the 2 columns that we need

my_dataframe = my_dataframe[['Tag','Tweet']]

print(my_dataframe)

my_dataframe.to_csv("Experimental_Data_csv.csv",index = False,header = True)

my_dataframe = pd.read_csv("Experimental_Data_csv.csv")

print(my_dataframe)

"""
print("Positive: %d\nNegative: %d\nNeutral: %d\n" % 
            (len(my_dataframe[my_dataframe['Tag'] == 'positive']),
            len(my_dataframe[my_dataframe['Tag'] == 'negative']),
            len(my_dataframe[my_dataframe['Tag'] == 'neutral'])))
"""

#COUNT example
tags = my_dataframe['Tag'].value_counts()
neutral_num = tags['neutral']
positive_num = tags['positive']
negative_num = tags['negative']

print("Positive Tweets Percentage %.1f" % ((positive_num / len(my_dataframe))*100))
print("Negative Tweets Percentage %.1f" % ((negative_num / len(my_dataframe))*100))
print("Neutral Tweets Percentage %.1f" % ((neutral_num / len(my_dataframe))*100))

# Printing the bar plot for tweets

tags.plot.bar(title = 'Tweets sentimel tendency')
plt.xticks(rotation = 0)

matplotlib.pyplot.show()

# EXAMPLE CLEANUP(CONVERTS TWEETS TO LOWER CASE)
my_dataframe['Tweet'] = my_dataframe.Tweet.apply(lambda t: t.lower())

##############################################

#TODO: SOME CLEANUP ON DATA TO GET BETTER RESULTS

cleaned_tweets = []

for tweet in my_dataframe['Tweet']:
    tweet = tweet.split(' ')

    tweet = filter(remove_unnecessary_words,tweet) # Removing hashtags,tags and links
    
    tweet = ' '.join(tweet)

    cleaned_tweets.append(tweet)

my_dataframe['Tweet'] = cleaned_tweets
cleaned_tweets = []

all_adjs_and_verbs = []
all_adjs_and_verbs_pos = []

all_adjs_and_verbs_neutral = []
neutral_adjs_and_verbs = set()

all_adjs_and_verbs_neg = []

for tweet_tag,tweet in zip(my_dataframe['Tag'],my_dataframe['Tweet']):
    original_tweet = tweet
    #for letter in tweet:
    #    if letter in my_punctuation:
    #        for char in my_punctuation:
    #            if letter == char:
    #                tweet = tweet.replace(char,' ')

    splitted_tweet = nltk.word_tokenize(tweet) 
    splitted_tweet = unify_negations(splitted_tweet)  #I do that because splits words like can't

    stemmer = PorterStemmer()

    cleaned_tweet = []

    unicode_start = '\\u'
    for word in splitted_tweet:             #Removing punctuation
        if unicode_start in word:
            index = word.index('\\')
            print(word)
            for i in range(5):
                index = index+1
                word = word.replace(word[index],' ')
            word.strip()
            print(word)

        cleaned_tweet.append(word.strip(my_punctuation))


            
    tweet = [ stemmer.stem(word) for word in cleaned_tweet if word not in nltk.corpus.stopwords.words('english')
                                                        and len(word) > 1]

    pos_tags = pos_tag(tweet)

    for tag in pos_tags: 
        if (tag[1][0] == 'J' or tag[1][0] == 'V') and len(tag[0]) > 1:
            all_adjs_and_verbs.append(tag[0])

            if tweet_tag == 'positive':
                if tag[0] not in neutral_adjs_and_verbs:
                    all_adjs_and_verbs_pos.append(tag[0])
            elif tweet_tag == 'negative':
                if tag[0] not in neutral_adjs_and_verbs:
                    all_adjs_and_verbs_neg.append(tag[0])
            else:
                all_adjs_and_verbs_neutral.append(tag[0])
                neutral_adjs_and_verbs.add(tag[0])
        
    tweet = ' '.join(tweet)

    
    cleaned_tweets.append(tweet)


"""
count = nltk.Counter(all_adjs_and_verbs)

most_common = count.most_common(20)

print(most_common)

plt.bar([x[0] for x in most_common],[x[1] for x in most_common],data = most_common)
plt.show()
"""

all_adjs_and_verbs_text = ' '.join(all_adjs_and_verbs)

cloud = wordcloud.WordCloud().generate(all_adjs_and_verbs_text)

plt.title("All Words")
plt.imshow(cloud,interpolation = 'bilinear')
plt.axis("off")
plt.show()

all_adjs_and_verbs_text = ' '.join(all_adjs_and_verbs_pos)

cloud = wordcloud.WordCloud().generate(all_adjs_and_verbs_text)

plt.title("Positive Words")
plt.imshow(cloud,interpolation='bilinear')
plt.axis("off")
plt.show()

all_adjs_and_verbs_text = ' '.join(all_adjs_and_verbs_neg)

cloud = wordcloud.WordCloud().generate(all_adjs_and_verbs_text)

plt.title("Negative Words")
plt.imshow(cloud,interpolation='bilinear')
plt.axis("off")
plt.show()

all_adjs_and_verbs_text = ' '.join(all_adjs_and_verbs_neutral)

cloud = wordcloud.WordCloud().generate(all_adjs_and_verbs_text)

plt.title("Neutral Words")
plt.imshow(cloud,interpolation='bilinear')
plt.axis("off")
plt.show()

my_dataframe['Tweet'] = cleaned_tweets
print(my_dataframe)
###############################################

# Getting the 20 most common words at negative tweets

negative_tweets_words = []

#sent_tokenize for sentence tokenize

eng_stopwords = nltk.corpus.stopwords.words('english')


for sentence in my_dataframe['Tweet']:#[my_dataframe.Tag == 'negative']:
    tokenized = sentence.split(' ')
    negative_tweets_words += tokenized

count = nltk.Counter(negative_tweets_words)

most_common = count.most_common(20)

print(most_common)

plt.bar([x[0] for x in most_common],[x[1] for x in most_common],data = most_common)
plt.show()

####################################

os.remove("Experimental_Data_csv.csv")

exp_datafile.close()